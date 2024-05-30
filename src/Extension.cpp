#include "Extension.h"

/// <summary>
/// Initate every function pointer with nullptr here. They will be assigned in the Device class once the device is
/// instanciated.
/// </summary>

PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR = nullptr;
