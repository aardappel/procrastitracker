DWORD g_dwLastXInputTick = 0; // tick time of last xinput event
int curr_freq = 0; // timer started with...

inline bool in_deadzone(SHORT input, SHORT deadzone) {
    return (input > -deadzone && input < deadzone);
}

void CALLBACK check_xinput_activity(PTP_CALLBACK_INSTANCE, PVOID, PTP_TIMER timer)
{
    XINPUT_STATE state = { 0 };
    for (int i = 0; i < MAXCTRLS; i++) {
        if (XInputGetState(i, &state) == ERROR_SUCCESS) {
            if (!(in_deadzone(state.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) &&
                  in_deadzone(state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) &&
                  in_deadzone(state.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) &&
                  in_deadzone(state.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) &&
                  in_deadzone(state.Gamepad.bLeftTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD) &&
                  in_deadzone(state.Gamepad.bRightTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD) &&
                  state.Gamepad.wButtons == 0)) {

                DWORD curtime = GetTickCount();
                CheckAway(g_dwLastXInputTick, curtime);
                g_dwLastXInputTick = curtime; // dword stores are threadsafe on x86
            }
        }
    }
}

PTP_TIMER xinputactivitytimer = CreateThreadpoolTimer(check_xinput_activity, NULL, NULL);

bool iscontrollerconnected() {
    for (int i = 0; i < MAXCTRLS; i++) {
        XINPUT_STATE state;
        if (XInputGetState(i, &state) == ERROR_SUCCESS) {
            return true;
        }
    }
    return false;
}

void start_xinput_activity_timer() {
    if (!xinputactivitytimer) {
        panic("Was unable to create threadpool timer!");
    }

    int freq = prefs[PREF_XINPUTACTIVITYFREQUENCY].ival;
    
    if (IsThreadpoolTimerSet(xinputactivitytimer) && freq == curr_freq) {
        return;
    }

    int timer_repeat_ms = 1000 / freq;

    // Microsoft why are you like this
    LONGLONG llDelay = -timer_repeat_ms * 10000LL;
    FILETIME ftDueTime = { (DWORD)llDelay, (DWORD)(llDelay >> 32) };

    SetThreadpoolTimer(xinputactivitytimer, &ftDueTime, timer_repeat_ms, 0); // never fails

    curr_freq = freq;
}

void stop_xinput_activity_timer() {
    SetThreadpoolTimer(xinputactivitytimer, NULL, 0, 0);
}
