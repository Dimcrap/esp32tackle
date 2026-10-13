        rbg led conroling with esp32

task1 -  adding a breathing mode .
Implement a new MODE_BREATHE that holds a single color (for example, warm white at hue 30)
        and slowly ramps brightness from 0 to 255 and back using the value parameter in HSV.
Use a step delay of about 15 ms for a
        smooth 8-second breathing cycle.

task2 - Add speed control commands. Accept “fast” and “slow” commands that change the animation delay. Store the delay in a shared variable protected by a mutex, or send it through a second queue. “fast” should halve the current delay, and “slow” should double it, clamped between 5 ms and 100 ms.


task3 - Print mode and core info on each change. In the animation task, print the new mode name, the current core ID (xPortGetCoreID()), and the current tick count (xTaskGetTickCount()) every time a mode change is received. This helps verify that tasks are actually running on their pinned cores.





