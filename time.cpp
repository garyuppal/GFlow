#include "Simulator.h"

void standardHopper(Simulator &simulation, int number) {
  simulation.discard();
  double left=0, right=1, bottom=0, top=3;
  simulation.setDimensions(left, right, bottom, top);
  double radius = 0.02;
  double gap = 0.14;
  double bottomGap = 0.05;
  double troughHeight = 0.5;
  double space = 1.0;
  double var = 0.25, mx = (1+var)*radius;
  simulation.addWall(new Wall(vect<>(0, troughHeight), vect<>(0,2*top), true));
  simulation.addWall(new Wall(vect<>(right, troughHeight), vect<>(right,2*top), true));
  simulation.addWall(new Wall(vect<>(0, troughHeight), vect<>(0.5-0.5*gap, bottomGap), true));
  simulation.addWall(new Wall(vect<>(1, troughHeight), vect<>(0.5+0.5*gap, bottomGap), true));
  simulation.addTempWall(new Wall(vect<>(0,troughHeight), vect<>(1,troughHeight), true), 3.0);
  double upper = 5; // Instead of top
  simulation.addNWParticles(number, radius, var, mx, right-mx, troughHeight+mx, upper-mx);
  simulation.setXLBound(WRAP);
  simulation.setXRBound(WRAP);
  simulation.setYTBound(NONE);
  simulation.setYBBound(RANDOM);

  simulation.setParticleDissipation(sphere_dissipation);
  simulation.setParticleCoeff(0);
  simulation.setParticleDrag(sphere_drag);
  simulation.setWallDissipation(wall_dissipation);
  simulation.setWallCoeff(wall_coeff);

  simulation.setDefaultEpsilon(1e-4);
  simulation.setMinEpsilon(1e-8);
}

int main() {

  Simulator simulation;

  ///***** Hopper tests ***********************************************************

  int number1 = 100;
  cout << "Hopper, " << number1 << " particles, 10 seconds" << endl;
  cout << "------------------------------------------------------------\n";

  srand(0);
  standardHopper(simulation, number1);
  simulation.setSectorDims(10,10);
  simulation.run(10);
  cout << "Sectors 10x10: " << simulation.getRunTime() << " s\n";
  cout << "Target: 5.4 s\n";
  cout << "Check: " << simulation.aveKE() << " (1.67722)\n\n";
  
  srand(0);
  standardHopper(simulation, number1);
  simulation.setSectorDims(5,5);
  simulation.run(10);
  cout << "Sectors 5x5: " << simulation.getRunTime() << " s\n";
  cout << "Target: 7.36 s\n";
  cout << "Check: " << simulation.aveKE() << " (1.46621)\n\n";
  
  srand(0);
  standardHopper(simulation, number1);
  simulation.setSectorize(false);
  simulation.run(10);
  cout << "No sectors: " << simulation.getRunTime() << " s\n";
  cout << "Target: 10.3 s\n";
  cout << "Check: " << simulation.aveKE() << " (1.53687)\n\n";
  
  return 0;
}

