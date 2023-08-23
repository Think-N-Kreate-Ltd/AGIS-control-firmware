    This branch is used for developing the usage of upper limited SW
    This branch is also used for adding limit to user input by keypad

===========================================================================
# below are developing the usage of upper limited SW
## Note:
- when the sensor sense no drop for 20s, auto-ctrl will do reposition
- the motor will then moving up without interval until find a new drop

## problem: motor moving up non-stop
- condition: 
    - when the container have no fluid
    - the sensor cannot sense a drop for 20
    - do reposition
    - keep moving up
    - reach the highest point
- details of problem:
    - at this time, the roller clamp does not allow the motor to go up. However, the program keep supplying power to motor
    - it is dangerous(destroy the motor), wasting power, and the sound is annoying
- solve:
    - add an obstacle to let the upper limited SW being touched at that time
    - the program will stop the motor when limited SW is touched

## feature: add a new state `ALARM_OUT_OF_FLUID`
- reach the state when the container running out of fluid
    - no drop for 28s
    - state = stopped (avoid it change from not started)
    - touching the upper limited SW
- do the following
    - disable auto-ctrl
    - homing
    - change the state to out of fluid
    - finish logging

## bugfix: cannot enter state exceeded
- condition:
    - when infusion completed, it will do homing
    - that time will also sense drops, not count then
    - when the time after homing
- details of bug:
    - at this time, sense drop
    - the value of volume and DR can change
    - but the time and state have no change
- reason:
    - while state = completed, time will not update, is not a bug
    - the condition millis()-recordTime>200 will never reach
    - it is because the time has just recorded s.t. it will always be 0
- solve:
    - directly use the state `motorHoming` is not a fix, because there may still a drop after homing completed
    - add a global var `homingCompletedTime` to get the time and +200ms as the condition

===========================================================================
# below are adding limit to user input by keypad

## the limit already have
- the input fields cannot input more than 4 numbers
- target DR should >=20 & <=400

## some condition of infusion that already known
- the container usually have size <3L only
- may have intermittent infusion (adminstered over a special period of time, and at a specific interval)
- may have a continuous infusion (delivered over a prolonged period of time, e.g. normal saline: serval days?)

## problem
- condition:
    - input all input fields
    - both hour and minutes input 0
- details of problem:
    - the program restart then
    - it should because of the DR becomes inf and the program crashed
- solving:
    - check in function `validate_keypad_inputs()`
    - when time = 0, not allow for auto-ctrl and msg alarm

## limitation: user should not change input when doing auto-ctrl
- details:
    - if change input when doing auto-ctrl
    - target DR may change
    - volume increase rate may change
    - target num drops will not change, which is used to change state to finish
- solving requirement
    - think of what can user do when started with unwanted input <- use `*` to stop immediately
    - disable user change the target value, or can only change when some condition satisify
- solving method:
    1. allow user input but will not submit and change
        - pre-input and can show change when `restart` <- not recommended
        - need one more set of var to store values <- can use struct but also need extra memory
        - need to pass `enableAutoControl` or add many state (as DF is set to change immediately to use direct control for infusion)
    2. cannot input when doing auto-ctrl
        - add a lock btn and freeze on it, the user cannot go to input field
        - need to find a pos to place the btn <- need to redesign the UI
        - when to finish it
- solving: use method (1)
    - add a btn on mon-scr, which will work as pressing `*` when pressed
    - cannot focus on the btn when not in auto-ctrl
    - when auto-ctrl, the user can only focus on the btn, which can block the user from input other values
    - nearly no extra mamory is used
    - the UI / text of the btn may need to change in the future (Note: text in the btn cannot scroll)

## feature: no limitation for user to choose drop factor
- condition:
    - now the drip factor can only be 10, 15, 20, 60
    - the user can only select 1 from that
- details:
    - user should be able to add new drip factor
    - but it may also cause safety problems
    - the infused volume may have error with new drip factor
- solve:
    - new drip factor can be added by adding a new element in array
    - refactor the volume
    - new drip factor cannot directly add in radio button, should update in a specific place, and a new radio button will appear after update <- not finish
    - maybe only user with access can add new drip factor <- not finish

## limitation: update target DR limit to <=20 and >=600
- condition:
    - user can have a auto-ctrl with DR range 20-400
    - 400 may be not enough
    - when user cannot start auto-ctrl because of DR, there is no msg telling them the reason
- change:
    - the user can have auto-ctrl with DR up to 600
    - the msg on top right will change to show the reason of not allowing auto-ctrl
    - after the msg is green colored, it can change back to red if input have problem

===========================================================================
# below are some hot fix
## fix: control motor will postpone homing
- condition:
    - when doing homing
    - at that time, the motor speed is set to very high
    - the time for hoimg should be should be short
    - means that user are not able to enter all input and run the auto-ctrl
    - but can directly control motor at that time
- details:
    - user can control the motor when doing homing (keypad move up/down)
    - the motor will then be overrided to move up/down
    - in fact, it cause no problem as the motor still doing homing after control finish, once homing is started, the will never be stopped until touch lower limited SW
    - besides, it is expected that the nurse will not do that as there is no reason for them to do so
    - However, we still don't want it to happen
- solve:
    - disable the motor control by keypad when doing homing

## fix: the comfirm msgbox pop up when in monitor screen
- condition and detail:
    - while in monitor screen, press F2, go back to input screen, msgbox pop up
    - because the newly added remind msgbox is outside the condition
- solve:
    - put the msgbox back to the condition
- extra problem: after press `back` btn, DF widget will scroll to right and focus on 100mL
- solve: add focus and scroll it back

## fix: the btn in monitor, press any key will trigger that
- see issue #42: https://github.com/Think-N-Kreate-Ltd/AGIS-control-firmware/issues/42
- follow up change: remove the state for msgbox `enterClicked` which is use to block the last event