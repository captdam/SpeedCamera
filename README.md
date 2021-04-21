# SpeedCamera

```
----------       ------------------       ---       ------------       -----------       ---       -------------
| Camera |  -->  | Edge Detection |  -->  |X|  -->  |Projection|  -->  | Compare |  -->  |X|  -->  | Speed Map |
----------       ------------------       ---       ------------       -----------       ---       -------------
                                     Accelerometer                                  Camera Position
```
- Camera:                     Record video, send video frame to next stage.
- Edge Detection:             Find the edge of object.
- Acceletometer & Projection: Compensate shifting of camera, so the image is always stable.
- Compare:                    Find the displacement of object edge between two frame.
- Camera Position:            Using camera position and pixel position to find object distance, then use displacement of object to find speed.
- Speed map:                  Speed of object saved in 2D map.
