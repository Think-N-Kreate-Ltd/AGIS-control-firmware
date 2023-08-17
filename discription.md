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
- target should >=20 & <=400
- thus, volume must <10000, time must <(10000/60=166h)

## some condition of infusion that already known
- the container usually have size <3L only
- may have intermittent infusion (adminstered over a special period of time, and at a specific interval)
- may have a continuous infusion (delivered over a prolonged period of time, e.g. saline: serval days)

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