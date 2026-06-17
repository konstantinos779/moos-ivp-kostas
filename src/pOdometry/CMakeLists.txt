#--------------------------------------------------------
# The CMakeLists.txt for:                       pOdometry
# Author(s):                              konstantinos779
#--------------------------------------------------------

SET(SRC
  Odometry.cpp
  Odometry_Info.cpp
  main.cpp
)

ADD_EXECUTABLE(pOdometry ${SRC})

TARGET_LINK_LIBRARIES(pOdometry
   ${MOOS_LIBRARIES}
   geometry
   apputil
   mbutil
   m
   pthread)

LINK_DIRECTORIES(
  ${MOOS_LIB_DIRS}
  ${IVP_LIB_DIRS}
  /home/kvosk/moos-ivp/lib
)
