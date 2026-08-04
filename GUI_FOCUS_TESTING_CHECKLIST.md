# GUI Focus & Performance Fix - Testing Plan

**Objective**: Verify that the GUI focus issue is fixed and performance is improved  
**Test Date**: [To be filled by tester]  
**Tester Name**: [To be filled by tester]  

---

## Pre-Test Setup

1. **Build Latest Firmware**
   ```bash
   cd D:\MCU\EspIDF\S3\uFlake
   idf.py build
   idf.py flash
   idf.py monitor
   ```

2. **Clear Serial Monitor** and set to 115200 baud
3. **Have Device Ready** - ESP32-S3 with display connected
4. **Note Firmware Build Time** for reference

---

## Test Suite 1: Basic Focus & Navigation (5 minutes)

### Test 1.1: Launcher Focus on Boot
**Expected**: First button is focused (highlighted in lighter color)

- [ ] Device boots
- [ ] Launcher app loads
- [ ] **OBSERVE**: Is the first app button visibly highlighted/focused?
  - [ ] YES - Focus is working ✅
  - [ ] NO - Focus issue still exists ❌

**Log Check**:
```
I (XXX) Launcher: Created X app buttons
I (XXX) Launcher: Requesting auto-focus for first button
I (XXX+50) uGUI: Auto-focus: Successfully focused first object: 0x3f...
```
Look for "Successfully focused" message in logs

**Result**: _____ PASS / FAIL

---

### Test 1.2: Navigation Up (In Launcher)
**Expected**: Up arrow focuses previous button (or wraps to last)

- [ ] In launcher with first button focused
- [ ] Press **UP** button on device
- [ ] **OBSERVE**: Does focus move to a different button?
  - [ ] YES - Navigation working ✅
  - [ ] NO - Navigation broken ❌
- [ ] Is there visible highlighting?
  - [ ] YES - Highlight clear ✅
  - [ ] NO - Highlighting not visible ❌

**Result**: _____ PASS / FAIL

---

### Test 1.3: Navigation Down (In Launcher)
**Expected**: Down arrow focuses next button (or wraps to first)

- [ ] Current focused button: _____ (note which one)
- [ ] Press **DOWN** button on device
- [ ] **OBSERVE**: Does focus move to next button?
  - [ ] YES - Navigation working ✅
  - [ ] NO - Navigation broken ❌
- [ ] Press **DOWN** again to verify cycling
  - [ ] YES - Smooth cycling ✅
  - [ ] NO - Stuck or jumping ❌

**Result**: _____ PASS / FAIL

---

### Test 1.4: No Lag During Navigation
**Expected**: Buttons respond immediately to presses

- [ ] Start rapid navigation (press UP/DOWN repeatedly for 10 seconds)
- [ ] **OBSERVE**: Is navigation responsive?
  - [ ] YES - Smooth, no lag ✅
  - [ ] NO - Laggy, slow to respond ❌
- [ ] Check logs for "GUI handler slow" warnings
  - [ ] NONE - Good ✅
  - [ ] Multiple - Performance issue ❌

**Log Pattern** (healthy):
```
I (9000) uGUI: GUI Health: iter=100, avg_sleep=48ms (21 calls/sec), timeouts=0, max_handler=9ms
```

**Result**: _____ PASS / FAIL

---

## Test Suite 2: App Launch & Exit (10 minutes)

### Test 2.1: Launch Settings App
**Expected**: Settings app opens with focus on first control

- [ ] In launcher with any button focused
- [ ] Press **OK** on a Settings app button
- [ ] **OBSERVE**: Does Settings app open?
  - [ ] YES - App launched ✅
  - [ ] NO - App didn't open ❌
- [ ] **OBSERVE**: Is first control focused (slider/button)?
  - [ ] YES - Auto-focus worked ✅
  - [ ] NO - Control not focused ❌

**Log Check**:
```
I (XXX) SettingsApp: Settings App Started
I (XXX) uGUI: Auto-focus: Successfully focused first object: 0x3f...
```

**Result**: _____ PASS / FAIL

---

### Test 2.2: Navigate in Settings App
**Expected**: Can navigate between controls with Up/Down

- [ ] In Settings app with focus on slider
- [ ] Press **DOWN** to move focus to next control
- [ ] **OBSERVE**: Does focus move to next control?
  - [ ] YES - Navigation working ✅
  - [ ] NO - Navigation broken ❌
- [ ] Press **UP** to move back
  - [ ] YES - Bidirectional works ✅
  - [ ] NO - Up doesn't work ❌

**Result**: _____ PASS / FAIL

---

### Test 2.3: Exit App with ESC
**Expected**: Pressing ESC closes app, returns to launcher

- [ ] In Settings app (any control focused)
- [ ] Press **ESC/BACK** button
- [ ] **OBSERVE**: Does app close?
  - [ ] YES - App closed ✅
  - [ ] NO - App still visible ❌
- [ ] **OBSERVE**: Is launcher displayed again?
  - [ ] YES - Launcher visible ✅
  - [ ] NO - Launcher not visible ❌

**Log Check**:
```
I (XXX) SettingsApp: Settings App Started
I (XXX) uGUI: Auto-focus: Successfully focused first object: ...
I (YYY) uGUI: ESC pressed - exiting current app
I (YYY+50) Launcher: Requesting auto-focus for first button
```

**Result**: _____ PASS / FAIL

---

### Test 2.4: Focus Restored to Launcher
**Expected**: After returning from app, launcher button is focused

- [ ] Exit Settings app (press ESC)
- [ ] **OBSERVE**: Is first launcher button focused?
  - [ ] YES - Focus restored ✅
  - [ ] NO - Focus lost ❌
- [ ] Can you navigate buttons immediately (without pressing one first)?
  - [ ] YES - Navigation ready ✅
  - [ ] NO - Need to press button first ❌

**Result**: _____ PASS / FAIL

---

## Test Suite 3: Focus Reliability (15 minutes)

### Test 3.1: Multiple App Launches
**Expected**: Focus works consistently across multiple launches

- [ ] Launch Settings app
- [ ] Verify first control is focused
- [ ] Press ESC to exit
- [ ] Verify launcher button is focused
- [ ] **REPEAT** for 5 different apps (or same app 5 times)
- [ ] **OBSERVE**: Does focus work every time?
  - [ ] ALL 5 times worked ✅ (100% reliability)
  - [ ] 4 times worked ⚠️ (80% reliability)
  - [ ] <3 times worked ❌ (unreliable)

**Result**: _____ PASS / FAIL (Success Rate: __/5)

---

### Test 3.2: Rapid Navigation & Launch
**Expected**: Even with rapid input, focus works correctly

- [ ] In launcher
- [ ] Rapidly press DOWN/DOWN/OK (launch without settling)
- [ ] App opens
- [ ] **OBSERVE**: Is first control focused?
  - [ ] YES - Handles rapid input ✅
  - [ ] NO - Missed focus ❌
- [ ] Repeat 3 times
- [ ] Any crashes?
  - [ ] NO - Stable ✅
  - [ ] YES - Crash ❌

**Result**: _____ PASS / FAIL

---

### Test 3.3: No Focus Lock-up
**Expected**: Device never becomes unresponsive due to focus issues

- [ ] Perform all previous tests
- [ ] Device should **never** require a hard reset
- [ ] Navigation should **never** completely freeze
- [ ] If it does happen, note the circumstance: _____________________

**Result**: _____ PASS / FAIL

---

## Test Suite 4: Performance Monitoring (10 minutes)

### Test 4.1: Idle Performance (Launcher Only)
**Expected**: GUI health logs show healthy idle state

- [ ] Boot device, let launcher run for 10 seconds
- [ ] **CAPTURE** GUI Health logs (lines with "GUI Health:")
- [ ] Paste below:
  ```
  [Log lines here]
  ```

**Analysis**:
- [ ] avg_sleep 30-50ms?  _____ YES ✅ / NO ❌
- [ ] calls/sec 15-35?    _____ YES ✅ / NO ❌
- [ ] timeouts ~0?        _____ YES ✅ / NO ❌
- [ ] max_handler <30ms?  _____ YES ✅ / NO ❌

**Interpretation**:
- If all YES → **Excellent performance** ✅
- If 3/4 YES → **Good performance** ✅
- If <3/4 YES → **Performance issue** ❌

**Result**: _____ PASS / FAIL

---

### Test 4.2: Animation Performance (Settings with Slider)
**Expected**: GUI health logs show adaptive timing

- [ ] Launch Settings app
- [ ] Interact with slider (move it up/down for 5 seconds)
- [ ] **CAPTURE** GUI Health logs
- [ ] Paste below:
  ```
  [Log lines here]
  ```

**Analysis**:
- [ ] avg_sleep 10-25ms?  _____ YES ✅ / NO ❌
- [ ] calls/sec 40-100?   _____ YES ✅ / NO ❌
- [ ] timeouts ~0?        _____ YES ✅ / NO ❌
- [ ] max_handler <50ms?  _____ YES ✅ / NO ❌

**Interpretation**:
- If all YES → **Animation performance great** ✅
- If 3/4 YES → **Acceptable** ✅
- If <3/4 YES → **Animation issue** ❌

**Result**: _____ PASS / FAIL

---

### Test 4.3: CPU Usage Comparison
**Expected**: Overall system less sluggish than before

- [ ] Compare feel of navigation to before the fix (if you know what it was like)
- [ ] Is navigation snappier?
  - [ ] YES - Noticeably faster ✅
  - [ ] MAYBE - Slightly faster ⚠️
  - [ ] NO - Same or slower ❌

**Result**: _____ PASS / FAIL

---

## Test Suite 5: Stability & Watchdog (15 minutes)

### Test 5.1: No Watchdog Resets
**Expected**: Device runs without watchdog timeouts

- [ ] Run device for 5+ minutes
- [ ] Continuously navigate, launch apps, exit
- [ ] **OBSERVE**: Any watchdog reset messages in logs?
  - [ ] NONE - Stable ✅
  - [ ] Some - Unstable ❌
- [ ] Any kernel panics?
  - [ ] NONE - Stable ✅
  - [ ] YES - Critical issue ❌

**Log Check**: Should see "Kernel loop running" every ~5 seconds

**Result**: _____ PASS / FAIL

---

### Test 5.2: System Stays Responsive
**Expected**: Device always responsive to input

- [ ] Run for 5+ minutes (see Test 5.1)
- [ ] Input should ALWAYS cause immediate response
- [ ] No "frozen" periods >0.5 seconds
- [ ] Can launch/exit apps anytime
  - [ ] YES - Always responsive ✅
  - [ ] NO - Sometimes hangs ❌

**Result**: _____ PASS / FAIL

---

### Test 5.3: Focus Never Corrupts State
**Expected**: No crashes when deleting/recreating UI

- [ ] Launch app 10 times rapidly
- [ ] No crashes
  - [ ] Correct ✅
  - [ ] Had crashes ❌
- [ ] No memory leaks (device doesn't slow down over time)
  - [ ] Correct ✅
  - [ ] Device gets slower ❌

**Result**: _____ PASS / FAIL

---

## Summary Results

| Test Suite | Result | Notes |
|-----------|--------|-------|
| 1.1 - Launcher Focus | PASS/FAIL | |
| 1.2 - Navigation Up | PASS/FAIL | |
| 1.3 - Navigation Down | PASS/FAIL | |
| 1.4 - No Lag | PASS/FAIL | |
| 2.1 - App Launch | PASS/FAIL | |
| 2.2 - App Navigation | PASS/FAIL | |
| 2.3 - App Exit | PASS/FAIL | |
| 2.4 - Focus Restored | PASS/FAIL | |
| 3.1 - Multiple Launches | PASS/FAIL | Success Rate: __/5 |
| 3.2 - Rapid Input | PASS/FAIL | |
| 3.3 - No Lock-up | PASS/FAIL | |
| 4.1 - Idle Perf | PASS/FAIL | |
| 4.2 - Animation Perf | PASS/FAIL | |
| 4.3 - CPU Comparison | PASS/FAIL | |
| 5.1 - Watchdog Stability | PASS/FAIL | |
| 5.2 - Responsiveness | PASS/FAIL | |
| 5.3 - State Integrity | PASS/FAIL | |

**Total**: ___/17 tests passed

**Overall Result**: 
- [ ] **ALL TESTS PASSED** ✅ - Ready for production
- [ ] **Minor Issues** ⚠️ - Need investigation
- [ ] **Critical Failures** ❌ - Rollback needed

---

## Issues Found

List any issues encountered:

1. Issue: _______________________________
   - Severity: LOW / MEDIUM / HIGH
   - Reproducible: YES / NO / SOMETIMES
   - Action: _____________________________

2. Issue: _______________________________
   - Severity: LOW / MEDIUM / HIGH
   - Reproducible: YES / NO / SOMETIMES
   - Action: _____________________________

---

## Notes & Observations

```
[Additional notes, observations, or concerns here]
```

---

## Tester Sign-Off

**Tester Name**: ___________________________  
**Date/Time**: ___________________________  
**Overall Assessment**: ___________________________  
**Approval**: [ ] APPROVED [ ] APPROVED WITH ISSUES [ ] REJECTED  

---

## Reviewer Sign-Off

**Reviewer Name**: ___________________________  
**Date/Time**: ___________________________  
**Comments**: ___________________________  
**Approval**: [ ] APPROVED [ ] NEEDS REWORK [ ] REJECTED  

---

*Test Plan Version 1.0 - February 2026*
