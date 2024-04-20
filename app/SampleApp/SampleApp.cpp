#include "Mandrill.h"

using namespace Mandrill;

class SampleApp : public App
{
public:
    SampleApp() : App("Sample App", 1280, 720)
    {
    }

    ~SampleApp(){};

    void execute() override
    {
        logInfo("Execute");
    }

    void renderUI() override
    {
        logInfo("renderUI");
    }

private:
};

int main()
{
    SampleApp app = SampleApp();
    app.run();
    return 0;
}
