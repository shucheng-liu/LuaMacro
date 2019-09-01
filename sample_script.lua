--[[	Name: Sample
--]]
-- DO NOT delete the following lines.
require('utility')
HOTKEY = VK_F8				-- the hotkey to start/stop the script (VK_* are defined in utility.lua)
IS_CONTINUOUS_RUN = false	-- repeatedly run the script
FinishEnvironmentSetting()
-- End

imageIdx = CaptureWindowImage()	-- capture screen
r, g, b = GetRgbOfPointOnImage(imageIdx, 100, 200)	-- get RGB value of the image on x=100, y=200
DeleteImage(imageIdx) -- you MUST delete an image captured by CaptureWindowImage()
MoveMouseToPoint(100, 200)	-- move cursor to the coordinates x=100 y=200
if r == 10 and g == 20 and b == 30 then
	MoveMouseToPoint(100, 200)	-- move cursor to the coordinates x=100 y=200
	Delay(100)	-- delay 100 ms
	MouseButton(MouseButton_Left, ButtonAction_Click)	-- click left mouse button (MouseButton_* and ButtonAction_* are defined in utility.lua)
	Delay(100)
	KeyboardButton(VK_S, ButtonAction_Click)	-- click S key
	Delay(100)
	KeyboardButton(VK_A, ButtonAction_Click)	-- click A key
	Delay(100)
	KeyboardButton(VK_M, ButtonAction_Click)	-- click M key
	Delay(100)
	KeyboardButton(VK_P, ButtonAction_Click)	-- click P key
	Delay(100)
	KeyboardButton(VK_L, ButtonAction_Click)	-- click L key
	Delay(100)
	KeyboardButton(VK_E, ButtonAction_Click)	-- click E key
end
