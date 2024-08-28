--
-- DESCRIPTION
--
-- @COMPANY **
-- @AUTHOR **
-- @DATE ${date} ${time}
--

---@type Lua_TestWidget_C
local M = UnLua.Class()
local PrintString = UE.UKismetSystemLibrary.PrintString
function Print(text, color, duration)
	color = color or UE.FLinearColor(1, 1, 1, 1)
	duration = duration or 100
	PrintString(nil, text, true, false, color, duration)
end

--function M:Initialize(Initializer)
--end

--function M:PreConstruct(IsDesignTime)
--end

function M:Construct()
	self.Button_40.OnClicked:Add(self, self.OnButtonClicked)
	self.TimerHandle = UE.UKismetSystemLibrary.K2_SetTimerDelegate({self, self.OnTimer}, 1, ture)
end

function M:OnButtonClicked()
	Print("OnButtonClicked")
end

function M:OnTimer()
	local seconds = UE.UKismetSystemLibrary.GetGameTimeInSeconds(self)
	Print("OnTimer Seconds "..seconds)
end

function M:Destruct()
	self.ClickMeButton.OnClicked:Remove(self, self.OnButtonClicked)
	UE.UKismetSystemLibrary.K2_ClearAndInvalidateTimerHandle(self, self.TimerHandle)
end

--function M:Tick(MyGeometry, InDeltaTime)
--end

return M
