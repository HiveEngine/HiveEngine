#pragma once

namespace terra
{
    //Opaque handle
    struct WindowContext;

    struct InputState
    {
        //Keyboard
        bool keys_[512]; //TODO have some easy getter for specific KEYS

        //Mouse
        bool mouseButton_[8];
        float mouseDeltaX;
        float mouseDeltaY; //Delta from last frame
        float mouseX;
        float mouseY; //Current mouse position on screen
    };


    //Windowing system init
    bool InitSystem();

    void ShutdownSystem();

    //Window operation
    bool InitWindowContext(WindowContext *windowContext);

    void ShutdownWindowContext(WindowContext *windowContext);

    bool ShouldWindowClose(WindowContext *windowContext);

    InputState *GetWindowInputState(WindowContext *windowContext);

    //Inputs
    void PollEvents(); //Called once per frame
}