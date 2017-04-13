/*
 * SUMO
 * SUMO.cc
 *
 * Purpose: Handles the connection to SUMO.
 * Since SUMO will only be used as a middleware for other simulators,
 * this module only transmits the current state of the simulation to it.
 * Other clients can use the TraCI API provided by SUMO to receive data.
 */

#include <sumo.hh>

// traci
#include <utils/traci/TraCIAPI.h>
// protobuf
#include <track.pb.h>
//
#include <math.h>
#include <string>

#define SUMO_IP "localhost"
#define SUMO_PORT 2002

/* TraCIAPI client library: http://sumo.dlr.de/wiki/TraCI/C%2B%2BTraCIAPI */

SUMO::SUMO(protobuf::Track &track) {
  this->track = track;

  try {
    TraCIAPI::connect(SUMO_IP, SUMO_PORT);
  } catch (tcpip::SocketException &e) {
    // TODO logging
    std::cout << "TraCIAPI: " << e.what() << std::endl;
  }

  /* calculate adjustment for x and y coordinates between SD2 and SUMO */
  TraCIAPI::TraCIPosition firstSegment = TraCIAPI::simulation.convert2D("0to1", 0);
  adjustX = track.segments(0).vertex(0).x() - (float)firstSegment.x;
  adjustY = track.segments(0).vertex(0).y() - (float)firstSegment.y - std::abs(track.segments(0).vertex(0).y() - track.segments(0).vertex(1).y()) / 2;
  //adjustY = track.segments(0).vertex(0).y() - (float)firstSegment.y + std::abs(track.segments(0).vertex(0).y() - track.segments(0).vertex(1).y()) / 2;

  /* start simulation */
  TraCIAPI::simulationStep(0);

  firstSegment = TraCIAPI::simulation.convert2D("0to1", 0);
  std::cout << "(" << (float)firstSegment.x << ", " << (float)firstSegment.y << ")" << std::endl;
};

SUMO::~SUMO() {
  try {
    TraCIAPI::close();
  } catch (tcpip::SocketException &e) {
    // TODO logging
    std::cout << "TraCIAPI: " << e.what() << std::endl;
  }
};

void SUMO::simulationStep(protobuf::Situation &situation) {
  float x, y, angle;

  for(int i=0; i < situation.vehicles_size(); i++) {
    protobuf::Vehicle veh = situation.vehicles(i);

    moveVehicleToXY(veh.name(),
                    veh.mutable_position()->x(),
                    veh.mutable_position()->y(),
                    veh.yaw());
  }

  TraCIAPI::simulationStep(situation.time() * 1000);
}

float SUMO::convertYawToAngle(float yaw) {
  // convert from yaw to angle
  float angle = yaw * 180.0 / M_PI;
  if (angle < 0) angle += 360.0;

  // SUMO uses different rotation
  if (angle > 270) angle = 450 - angle;
  else angle = 90 - angle;

  return angle;
};

void SUMO::moveVehicleToXY(std::string name, float x, float y, float angle) {
  // adjust (x, y) coordinates
  x = x - SUMO::adjustX;
  y = y - SUMO::adjustY;

  // workaround to fix some vehicle misplacements
  std::tuple<std::string, SUMOReal, int> tmp = TraCIAPI::simulation.convertRoad(x, y);

  // convert yaw to angle and adjust
  angle = SUMO::convertYawToAngle(angle);

  // execute moveToXY
  while(true) {
    try {
      TraCIAPI::vehicle.moveToXY(name, std::get<0>(tmp), std::get<2>(tmp), x, y, angle, 2);
      break;
    } catch (tcpip::SocketException &e) {
      // vehicle disappeared? or wasn't part of the simulation
      std::cout << "TraCIAPI: " << e.what() << std::endl;
      TraCIAPI::vehicle.add(name, "route0", "Car", std::to_string(TraCIAPI::simulation.getCurrentTime()));
    }
  }
};
