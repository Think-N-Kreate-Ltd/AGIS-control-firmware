    This branch is used for developing the usage of upper limited SW

## Note:
- when the sensor sense no drop for 20s, auto-ctrl will do reposition
- the motor will then moving up without interval until find a new drop

## problem: motor moving up non-stop
- condition: 
    - when the container have no field
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

## feature: add a new state `ALARM_OUT_OF_FIELD`
- reach the state when the container running out of field
    - no drop for 28s
    - state = stopped (avoid it change from not started)
    - touching the upper limited SW
- do the following
    - disable auto-ctrl
    - homing
    - change the state to out of field
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