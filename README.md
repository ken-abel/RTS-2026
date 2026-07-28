A flight control system that handles priority inversion, built to demonstrate the importance of proper mutex implementation in an
aerospace embedded system.

A small drone, deployed from a nearby spacecraft currently orbiting Mars, needs to adjust its attitude and trajectory, map the surface, 
and routinely log its flight path to a database located on the larger spacecraft.

| Task             | Priority | Function                                                     |
| ---------------- | -------- | ------------------------------------------------------------ |
| Flight Control   | High     | Computes engine thrust and attitude corrections every 10 ms. | Note: this period has been slowed to 1s for the sake of demonstrating priority inversion
| Terrain Mapping  | Medium   | Processes camera and lidar data to avoid hazards.            |
| Telemetry Logger | Low      | Uploads flight data to nearby host ship.                     |

The Telemetry Logger locks the navigation mutex to upload current positional data to the nearby spacecraft.
The Flight Control task periodically becomes ready each control cycle, requiring the navigation mutex to operate.
The Terrain Mapping task processes lidar and camera frames upon manual request from the nearby spacecraft (simulated by pressing the button). It
is very time and resource heavy.

The Flight Control task needs to be able to quickly respond to disturbances or detected obstacles and hazards. Due to the presence
of the mutex and the given priority hierarchy, priority inversion is possible. So, it is important to enable priority inheritance so the Terrain 
Mapping task doesn't preempt the Telemetry Logger while the Flight Control task is urgently waiting for the mutex.
To simulate having priority inheritance disabled, a binary semaphore can be used in place of a mutex by setting USE_PI_MUTEX to 0.


Capabilities from each application:
App 4 is the skeleton. Specifically, the priority inversion demo.
Folded in the LED and vTaskDelay functionality from App 1,
as well as the binary semaphore signaling from App 3 to enable signaling
H and M tasks to start (H via vTaskDelayUntil loop from App 1, M via GPIO
interrupts and binary semaphore signaling).
