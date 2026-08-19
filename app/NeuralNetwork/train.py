"""Train a small MLP to reproduce an image, and write it in the format Mandrill's MLP class reads.

The network is a neural image field: it maps a 2D coordinate in [0, 1] to an RGB colour. The coordinate is expanded
into sines and cosines of a few octaves before it reaches the network, which is what lets an MLP this small hold on to
detail that raw coordinates would smooth away. The same expansion is done in NeuralImage.frag, so encode() here and
encode() there have to stay in step.

Usage:

    python train.py                       # fit res/textures/icon.png, write res/networks/icon.mlp
    python train.py --image my.png --output my.mlp

Needs torch, numpy and Pillow.
"""

import argparse
import struct
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn
from PIL import Image

# Version of the file format, must match MLP::kFileVersion in src/MLP.h
MLP_FILE_VERSION = 1


def encode(coords, frequencies):
    """Expand coordinates into Fourier features.

    The layout of the result is what NeuralImage.frag fills its input vector with:

        [x, y, sin(f0*x), sin(f0*y), cos(f0*x), cos(f0*y), sin(f1*x), ...]

    with f_i = 2^i * pi.
    """
    features = [coords]

    for i in range(frequencies):
        f = (2.0**i) * torch.pi * coords
        features.append(torch.sin(f))
        features.append(torch.cos(f))

    return torch.cat(features, dim=-1)


class MLP(nn.Module):
    """A plain stack of linear layers with ReLU in between and a sigmoid on the output.

    Mandrill's MLP class runs the hidden layers in a loop on a cooperative vector of one fixed width, so every hidden
    layer has to be the same width. The sigmoid is applied by the shader, not stored in the file.
    """

    def __init__(self, inputs, outputs, width, layers):
        super().__init__()

        modules = [nn.Linear(inputs, width), nn.ReLU()]

        for _ in range(layers - 2):
            modules += [nn.Linear(width, width), nn.ReLU()]

        modules += [nn.Linear(width, outputs)]

        self.network = nn.Sequential(*modules)

    def forward(self, x):
        return torch.sigmoid(self.network(x))

    def save(self, path):
        linears = [m for m in self.network if isinstance(m, nn.Linear)]

        content = bytearray()
        content.extend(struct.pack("I", MLP_FILE_VERSION))
        content.extend(struct.pack("I", len(linears)))

        for linear in linears:
            weight = linear.weight.data.to("cpu").numpy().astype("float32")
            bias = linear.bias.data.to("cpu").numpy().astype("float32")

            # Weight is [out_features, in_features], written row major
            content.extend(struct.pack("II", weight.shape[0], weight.shape[1]))
            content.extend(weight.tobytes())

            content.extend(struct.pack("I", bias.shape[0]))
            content.extend(bias.tobytes())

        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, "wb") as file:
            file.write(content)


def load_image(path, background):
    """Load an image as an (H, W, 3) array in [0, 1], composited over a flat background if it has alpha."""
    image = Image.open(path).convert("RGBA")
    rgba = np.asarray(image, dtype=np.float32) / 255.0

    rgb = rgba[..., :3]
    alpha = rgba[..., 3:]

    return rgb * alpha + background * (1.0 - alpha)


def main():
    root = Path(__file__).resolve().parents[2]

    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--image", type=Path, default=root / "res" / "textures" / "icon.png",
                        help="image to fit the network to")
    parser.add_argument("--output", type=Path, default=root / "res" / "networks" / "icon.mlp",
                        help="where to write the network")
    parser.add_argument("--background", type=float, default=0.1,
                        help="grey level the image is composited over, must match BACKGROUND in NeuralImage.frag")
    parser.add_argument("--frequencies", type=int, default=8, help="octaves of Fourier features")
    parser.add_argument("--width", type=int, default=64, help="neurons per hidden layer")
    parser.add_argument("--layers", type=int, default=4, help="linear layers, counting input and output")
    parser.add_argument("--steps", type=int, default=3000, help="training steps")
    parser.add_argument("--batch", type=int, default=16384, help="pixels per step")
    parser.add_argument("--lr", type=float, default=5e-3, help="initial learning rate")
    parser.add_argument("--seed", type=int, default=0, help="seed for the random number generator")
    parser.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu",
                        help="device to train on")
    args = parser.parse_args()

    torch.manual_seed(args.seed)
    device = torch.device(args.device)

    image = load_image(args.image, args.background)
    height, width = image.shape[:2]
    print(f"Fitting {args.image} ({width}x{height}) on {device}")

    # One training sample per pixel, at the centre of the pixel so that the coordinates stay inside [0, 1] the way
    # the shader's texture coordinates do
    ys, xs = np.meshgrid(
        (np.arange(height, dtype=np.float32) + 0.5) / height,
        (np.arange(width, dtype=np.float32) + 0.5) / width,
        indexing="ij",
    )
    coords = torch.from_numpy(np.stack([xs, ys], axis=-1).reshape(-1, 2)).to(device)
    targets = torch.from_numpy(image.reshape(-1, 3)).to(device)

    inputs = encode(coords, args.frequencies)

    model = MLP(inputs.shape[1], targets.shape[1], args.width, args.layers).to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr)
    scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=args.steps, eta_min=args.lr / 100.0)

    batch = min(args.batch, inputs.shape[0])

    for step in range(args.steps):
        indices = torch.randint(0, inputs.shape[0], (batch,), device=device)

        loss = torch.nn.functional.mse_loss(model(inputs[indices]), targets[indices])

        optimizer.zero_grad(set_to_none=True)
        loss.backward()
        optimizer.step()
        scheduler.step()

        if step % 200 == 0 or step == args.steps - 1:
            print(f"step {step:5d}  loss {loss.detach().item():.6f}")

    # How well the fit turned out over the whole image, the shader will be slightly worse since it runs the
    # network in half precision
    with torch.no_grad():
        prediction = torch.cat([model(inputs[i:i + 65536]) for i in range(0, inputs.shape[0], 65536)])
        mse = torch.nn.functional.mse_loss(prediction, targets).item()
        print(f"final MSE {mse:.6f}, PSNR {-10.0 * np.log10(max(mse, 1e-12)):.2f} dB")

    model.save(args.output)
    print(f"Wrote {args.output} "
          f"({inputs.shape[1]} inputs, {targets.shape[1]} outputs, {args.layers} layers of width {args.width})")


if __name__ == "__main__":
    main()
