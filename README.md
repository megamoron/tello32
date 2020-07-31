# tello32
Fly the Ryze Tech Tello by DJI with a gamepad attached to your PC. It works with [XInput](https://docs.microsoft.com/en-us/windows/win32/xinput/xinput-game-controller-apis-portal) devices or [DirectInput](https://docs.microsoft.com/en-us/previous-versions/windows/desktop/ee416842(v=vs.85)) devices and uses the [Tello SDK](https://dl-cdn.ryzerobotics.com/downloads/Tello/Tello%20SDK%202.0%20User%20Guide.pdf).

Only the most basic controls are supported (no video, no flight modes, etc.).

## Controls

The controls are in mode 2. Press Y to takeoff and A to land. Pressing B will query the battery status and cause it to be displayed in the console window. When all four shoulder buttons are pressed simultaneously, the drone will fall to the ground.

**Important:** The drone needs to be set to SDK mode before it will process commands. However, the mode resets when the drone is turned off. Since this program enables SDK mode only during startup, you need to restart this program whenever the drone is turned off.

## Building
###### Visual Studio

As Microsoft Visual Studio is unable to create a solution in an existing directory, the solution/project needs to be created first after which the code can be cloned into the solution directory (as described [here](https://stackoverflow.com/questions/2411031/how-do-i-clone-into-a-non-empty-directory)) and the resulting files can then be added to the project.