    This branch is used for checking the statement & condition coverage
    Also, will make a quick fix and mark them down here

1. ## the `dripRate` (drip rate, DR) calaulation seems doing too much and redundant
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
        - by calaulation, we have: 0.94ms/loop
    - it is claerly to see that it calculates DR too many times, which is redundant. As a result, the time spend on calculation is also wasted
    - on the other hand, some there are also some unnessary calaulation, such as when infusion is not started and completed.
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
        - by calculation, we have: 0.89ms/loop
    - NOTE: no drop 20s works normally
    - ## short conclude: 
    - when placing the DR calculation to EXT INT, the time of calculation have not change much but the coverage reduce a lot, which helps to improve the program efficiency, with the same outcomes.
    - the reduction mainly comes from remove the redundant & unnecessary update (when infusion not started yet, and repeated calculation)
    - PROBLEM: one other extra problem discoverd is: complete by `*` cause the infusion state cannot go back to not started. it should because of `firstDropDected` is reset to false, which blocks to meet the condition to do no drop 20s
    - fixed by keeping the `firstDropDected` as true