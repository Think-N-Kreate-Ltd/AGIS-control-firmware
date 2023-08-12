    This branch is used for checking the statement & condition coverage
    Also, will make a quick fix and mark them down here

1. ## the `dripRate` (drip rate, DR) calculation seems doing too much and redundant
    - measure the total time and count of calling DR calculation
    - under below condition
        - drip factor = 20 (in normal case)
        - target DR = 300 (normally will not exist this number)
        - time = 5 mins
        - volume = 75
    - expected result:
        - count = 5min * 60s * (1000/5) = 60000
    - result: run out of water so have not finish
        - another unexpected problem: counting before start infusion
        - means that there are already redundand calculation
        - count from 43XX to 65387 (~61000), XXXX to 61642(micros)
        - time = 5min
        - volume = 72.95 (drops = 1459)
        - by calculation, we have: 0.94ms/loop
    - it is claerly to see that it calculates DR too many times, which is redundant. As a result, the time spend on calculation is also wasted
    - on the other hand, some there are also some unnessary calculation, such as when infusion is not started and completed.
    - one other extra problem discoverd is: complete by `*` cause the infusion state cannot go back to not started
    - ## solve:
    - move the DR calculation to EXT INT, to only do calculation when sense a drop
    - add reset DR value when no drops 20s
    - testing condition same as above
    - ecpexted result:
        - count = 1500 (drops number)
        - time = 1500 * 0.94 = 1410
    - result:
        - count = 1504, 1334
        - time = 4min59sec
        - volume = 75.20 (drops = 1504)
        - by calculation, we have: 0.89us/loop
    - NOTE: no drop 20s works normally
    - ## short conclude: 
    - when placing the DR calculation to EXT INT, the time of calculation have not change much but the coverage reduce a lot, which helps to improve the program efficiency, with the same outcomes.
    - the reduction mainly comes from remove the redundant & unnecessary update (when infusion not started yet, and repeated calculation)
    - PROBLEM: one other extra problem discoverd is: complete by `*` cause the infusion state cannot go back to not started. it should because of `firstDropDected` is reset to false, which blocks to meet the condition to do no drop 20s
    - fixed by keeping the `firstDropDected` as true

2. ## RAM usage of `wm`
    - only a task will use it but it is in the global
    - also, it can also be deleted after wifi enabled
    - both duration and scope is over used
    - place at global: 
        - RAM: 115904(35.4%), Flash: 1306653(39.1%)
    - place at the used task:
        - RAM: 115288(35.2%), Flash: 1306721(39.1%)
    - we can clearly see that it is better to place it at task

3. ## the `dropSensorState` (sensor OUT) get value seems doing too much and redundant
    - measure the total time and count of getting sersor OUT
    - under below condition
        - drip factor = 20 (in normal case)
        - target DR = 300 (normally will not exist this number)
        - time = 5 mins
        - volume = 75
    - expected result:
        - count = (5min * 60s +20s(completed to not started)) * (1000/5) = 64000
    - result: run out of water so have not finish
        - another unexpected problem: counting before start infusion
        - means that there are already redundand calculation
        - count from 51XX to 69808 (~64000), XXXX to 186895(micros)
        - time = 4min59s
        - volume = 75.15 (drops = 1503)
        - by calculation, we have: 2.68us/loop
        - RAM: 115296(35.2%), Flash: 1306821(39.1%)
    - it is claerly to see that getting the value takes time, it is better to reduce it as much as possible, while the update dose too frequently
    - on the other hand, some there are also some unnessary calculation, such as when infusion is not started and completed.
    - ## solve:
    1. move `dropSensorState` to global var, share the value in EXT INT
        - will increase RAM use (but it is just a bool, very little use)
            - update: can make use of `dropState`
        - convenience and fast
    2. move the statement no drop 20s to EXT INT, s.t. the state can be removed
        - don't need to add en extra global var
        - need to move statement, which may cause other problems, but can reduce the condition checking coverage
        - EXT INT cannot check time frequently, need calculation for 20s, which may takes time
        - need to pass the time to the task, takes more RAM than `dropSensorState`
    - use method(1) currently
        - result: the getting sensor OUT is removed, so counting is 0,0
        - RAM: 115296(35.2%), Flash: 1306753(39.1%)
    - ## short conclude:
    - now use method(1), with no problem, and no extra RAM used
    - later may change to use method(2) for reducing the 20s condition checking coverage. As the logic need to change a lot, it is not done yet