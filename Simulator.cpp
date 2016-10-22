#include "Simulator.h"

Simulator::Simulator() : lastDisp(0), dispTime(1./15.), dispFactor(1), time(0), iter(0), bottom(0), top(1.0), yTop(1.0), left(0), right(1.0), minepsilon(default_epsilon), gravity(vect<>(0, -3)), markWatch(false), startRecording(0), stopRecording(1e9), startTime(1), delayTime(5), maxIters(-1), recAllIters(false), runTime(0), recIt(0), temperature(0), samplePoints(100), resourceDiffusion(50.), wasteDiffusion(50.), secretionRate(1.), eatRate(1.), recFields(false), replenish(0), wasteSource(0) {
  // Flow
  hasDrag = true;
  flowFunc = 0;
  // Time steps
  default_epsilon = 1e-4;
  epsilon = default_epsilon;
  min_epsilon = 1e-7;
  adjust_epsilon = false;
  // Boundary conditions
  xLBound = WRAP;
  xRBound = WRAP;
  yTBound = WRAP;
  yBBound = WRAP;
  // Sectorization
  sectorize = true;
  ssecInteract = false;
  secX = 10; secY = 10;
  sectors = new list<Particle*>[(secX+2)*(secY+2)+1];
  // Velocity analysis
  vbins = 200;
  maxF = 3.25;
  maxV = 2.; // flowV is zero
  velocityDistribution = vector<double>(vbins,0);
  auxVelocityDistribution = vector<double>(vbins,0);
};

Simulator::~Simulator() {
  for (auto P : particles) 
    if (P) {
      delete P;
      P = 0;
    }
  for (auto W : walls)
    if (W) {
      delete W;
      W = 0;
    }
  if (sectors) {
    delete [] sectors;
    sectors = 0;
  }
}

void Simulator::createSquare(int N, double radius) {
  discard();
  gravity = Zero;
  xLBound = WRAP;
  xRBound = WRAP;
  yTBound = WRAP;
  yBBound = WRAP;

  charRadius = radius;
  left = bottom = 0;
  top = right = 1;

  addParticles(N, radius, 0, left+0.5*radius, right-0.5*radius, bottom+0.5*radius, top-0.5*radius, PASSIVE, 1);

  setParticleDrag(0);
}

void Simulator::createHopper(int N, double radius, double gap, double width, double height, double act) {
  discard();
  // Set up a hopper
  charRadius = radius;
  left = 0; bottom = 0;
  right = width; top = height;

  double bottomGap = 2*radius;
  double troughHeight = 0.5*width; // Keep the angle at 45 degrees
  double space = 1.0;
  double var = 0, mx = (1+var)*radius;
  addWall(new Wall(vect<>(0, troughHeight), vect<>(0,2*top)));
  addWall(new Wall(vect<>(right, troughHeight), vect<>(right,2*top)));
  addWall(new Wall(vect<>(0, troughHeight), vect<>(0.5*right-0.5*gap, bottomGap)));
  addWall(new Wall(vect<>(right, troughHeight), vect<>(0.5*right+0.5*gap, bottomGap)));
  addTempWall(new Wall(vect<>(0,troughHeight), vect<>(right,troughHeight)), 3.0);

  double upper = 5; // Instead of top
  act = act>1 ? 1 : act;
  act = act<0 ? 0 : act;
  addParticles(act*N, radius, var, mx, right-mx, troughHeight+mx, upper-mx, RTSPHERE); // Active particles
  addParticles((1-act)*N, radius, var, mx, right-mx, troughHeight+mx, upper-mx); // Passive particles
  xLBound = WRAP;
  xRBound = WRAP;
  yTBound = NONE;
  yBBound = RANDOM;
  // Set physical parameters
  setParticleCoeff(0); // --> No torques, shear forces
  setParticleDissipation(sphere_dissipation);
  setParticleDrag(sphere_drag);
  setWallDissipation(wall_dissipation);
  setWallCoeff(wall_coeff);
  
  int sx = (int)(width/(2*(radius+var))), sy = (int)(top/(2*(radius+var)));
  setSectorDims(sx, sy);

  // Set where particles will be reinserted
  yTop = troughHeight + 1.3*particles.size()*PI*sqr(radius)/width; // Estimate where the top will be

  // Set neccessary times
  default_epsilon = 1e-4;
  min_epsilon = 1e-8;
}

void Simulator::createPipe(int N, double radius, double V, int NObst) {
  discard();
  gravity = Zero;
  charRadius = radius;
  left = 0; bottom = 0;
  top = 2; right = 5;

  addWall(new Wall(vect<>(0,0), vect<>(right,0)));
  addWall(new Wall(vect<>(0,top), vect<>(right,top)));
  // Add stationary obstacles
  addParticles(NObst, 2*radius, 0, 0, right, 0, top);
  setParticleFix(true);
  // Add mobile particles
  addParticles(N, radius, 0, 0, right, 0, top);
  xLBound = WRAP;
  xRBound = WRAP;
  yTBound = NONE;
  yBBound = NONE;

  setFlowV(V);
  flowFunc = [&] (vect<> pos) { return vect<>(flowV*(1-sqr(pos.y-0.5*(top-bottom)))/sqr(0.5),0); };
  hasDrag = true;
  for (auto P : particles) P->setVelocity(flowFunc(P->getPosition()));

  // Set physical parameters
  setParticleCoeff(0); // --> No torques, shear forces
  setParticleDissipation(sphere_dissipation);
  setParticleDrag(sphere_drag);
  setWallDissipation(wall_dissipation);
  setWallCoeff(wall_coeff);

  default_epsilon = 1e-4;
  min_epsilon = 1e-8;
}

void Simulator::createControlPipe(int N, int A, double radius, double V, double F, double rA, double width, double height, double runT, double tumT, double var, vect<> bias) {
  discard();
  gravity = Zero;
  charRadius = radius;
  left = 0; bottom = 0;
  top = height; right = width;
  if (rA==-1) rA = radius;
  
  // Set sample points
  samplePoints = 1.1547*(top-bottom)/(2*radius);

  addWall(new Wall(vect<>(0,bottom), vect<>(right,bottom))); // Bottom wall
  addWall(new Wall(vect<>(0,top), vect<>(right,top))); // Top wall

  // Use the maximum number of sectors
  double R = max(radius, rA);
  int sx = (int)(width/(2*(R+var))), sy = (int)(top/(2*(R+var)));
  setSectorDims(sx, sy);
  vector<vect<> > pos = findPackedSolution(N+A, radius, 0, right, 0, top);
  int i;
  // Add the particles in at the appropriate positions
  for (i=0; i<A; i++) addWatchedParticle(new RTSphere(pos.at(i), rA, F, runT, tumT, bias));
  for (; i<N+A; i++) addWatchedParticle(new Particle(pos.at(i), radius));
  
  xLBound = WRAP;
  xRBound = WRAP;
  yTBound = NONE;
  yBBound = NONE;

  setFlowV(V);
  flowFunc = [&] (vect<> pos) { return vect<>(flowV*(1-sqr(pos.y-0.5*(top-bottom))/sqr(0.5*(top-bottom))),0); };
  hasDrag = true;
  for (auto P : particles) P->setVelocity(flowFunc(P->getPosition()));

  // Set physical parameters
  setParticleCoeff(0); // --> No torques, shear forces
  setParticleDissipation(sphere_dissipation);
  setParticleDrag(sphere_drag);
  setWallDissipation(wall_dissipation);
  setWallCoeff(wall_coeff);
  
  default_epsilon = 1e-4;
  min_epsilon = 1e-8;
}

void Simulator::createIdealGas(int N, double radius, double v) {
  discard();
  gravity = Zero;
  charRadius = radius;
  left = 0; right = 1;
  bottom = 0; top = 1;

  addWall(new Wall(vect<>(0,0), vect<>(right,0))); // bottom
  addWall(new Wall(vect<>(0,top), vect<>(right,top))); // top
  addWall(new Wall(vect<>(0,0), vect<>(0,top))); // left
  addWall(new Wall(vect<>(right,0), vect<>(right,top))); // right

  addParticles(N, radius, 0, 0, right, 0, top, PASSIVE, v);
  double diss = 1.17;
  setParticleCoeff(0);
  setParticleDrag(0);
  setWallDissipation(diss);
  setParticleDissipation(diss);
  setWallDissipation(0);
  setWallCoeff(0);

  min_epsilon = 1e-5;
  default_epsilon = 1e-5;

  // epsilon 1e-5 -> dissipation 30
}

void Simulator::createEntropyBox(int N, double radius) {
  discard();
  gravity = Zero;
  double gap = 0.1;
  charRadius = radius;
  left = 0; right = 1;
  bottom = 0; top = 1;

  addWall(new Wall(vect<>(0,0), vect<>(right,0))); // bottom
  addWall(new Wall(vect<>(0,top), vect<>(right,top))); // top
  addWall(new Wall(vect<>(0,0), vect<>(0,top))); // left
  addWall(new Wall(vect<>(right,0), vect<>(right,top))); // right
  addWall(new Wall(vect<>(0.5,0), vect<>(0.5,0.5*(1.0-gap)))); // bottom partition
  addWall(new Wall(vect<>(0.5,1), vect<>(0.5,0.5*(1.0+gap)))); // bottom partition

  addParticles(N/2, radius, 0, radius, 0.5-radius, radius, top-radius, PASSIVE, 1);
  addParticles(N/2, radius, 0, 0.5+radius, 1-radius, radius, top-radius, PASSIVE, 0.1);
  setParticleDissipation(0);
  setParticleCoeff(0);
  setParticleDrag(0);
  setWallDissipation(0);
  setWallCoeff(0);
}

void Simulator::createBacteriaBox(int N, double radius, double width, double height, double V) {
  discard();
  gravity = Zero;
  charRadius = radius;
  left = 0; bottom = 0;
  top = height; right = width;

  // Set sample points
  samplePoints = 1.1547*(top-bottom)/(2*radius);

  addWall(new Wall(vect<>(0,bottom), vect<>(right,bottom))); // Bottom wall
  addWall(new Wall(vect<>(0,top), vect<>(right,top))); // Top wall

  // Use the maximum number of sectors
  int sx = (int)(width/(2*radius)), sy = (int)(top/(2*radius));
  setSectorDims(sx, sy);
  // Find packed solution
  vector<vect<> > pos = findPackedSolution(N, radius, 0, right, 0, top);
  // Add the particles in at the appropriate positions
  for (int i=0; i<N; i++) addWatchedParticle(new Bacteria(pos.at(i), radius));

  xLBound = WRAP;
  xRBound = WRAP;
  yTBound = NONE;
  yBBound = NONE;

  setFlowV(V);
  flowFunc = [&] (vect<> pos) { return vect<>(flowV*(1-sqr(pos.y-0.5*(top-bottom))/sqr(0.5*(top-bottom))),0); };
  hasDrag = true;
  for (auto P : particles) P->setVelocity(flowFunc(P->getPosition()));

  // Set physical parameters
  setParticleCoeff(0); // --> No torques, shear forces
  setParticleDissipation(0); // --> No dissipation for now
  setParticleDrag(sphere_drag);
  setWallDissipation(wall_dissipation);
  setWallCoeff(wall_coeff);

  default_epsilon = 1e-4;
  min_epsilon = 1e-8;
}

bool Simulator::wouldOverlap(vect<> pos, double R) {
  if (pos.x-R<left || right<pos.x+R || pos.y-R<bottom || top<pos.y+R) return true;
  for (auto P : particles) {
    if (P) {
      vect<> displacement = P->getPosition()-pos;
      double minSepSqr = sqr(R + P->getRadius());
      if (displacement*displacement < minSepSqr) return true;
    }
  }
  return false;
}

void Simulator::run(double runLength) {
  //Reset all neccessary variables for the start of a run
  resetVariables();
  // Run the simulation
  clock_t start = clock();
  // Initial record of data
  if (time>=startRecording && time<stopRecording || recAllIters) record();
  while(time<runLength && running) { // Terminate based on internal condition
    // Gravity, flow, particle-particle, and particle-wall forces
    calculateForces();
    // Time, iteration, and data recording
    logisticUpdates(); 
    // Update particles, sectors, and temp walls
    objectUpdates();
  }
  clock_t end = clock();
  runTime = (double)(end-start)/CLOCKS_PER_SEC;
}

void Simulator::bacteriaRun(double runLength) {
  //Reset all neccessary variables for the start of a run
  resetVariables();
  // Create waste, resource, and auxilary fields
  resource.setDims(secX-2,secY-2); waste.setDims(secX-2,secY-2); buffer.setDims(secX-2,secY-2);
  // Set field wrapping
  resource.setWrapX(xLBound==xRBound && xRBound==WRAP); resource.setWrapY(yTBound==yBBound && yBBound==WRAP);
  waste.setWrapX(xLBound==xRBound && xRBound==WRAP); waste.setWrapY(yTBound==yBBound && yBBound==WRAP);
  buffer.setWrapX(xLBound==xRBound==WRAP); buffer.setWrapY(yTBound==yBBound==WRAP);
  // Initialize the values of the waste and resource fields
  initializeFields();
  // Run the simulation
  clock_t start = clock();
  // Initial record of data
  if (time>=startRecording && time<stopRecording || recAllIters) record();
  while(time<runLength && running) { // Terminate based on internal condition
    // Gravity, flow, particle-particle, and particle-wall forces
    calculateForces();
    // Time, iteration, and data recording
    logisticUpdates();
    // Update particles, sectors, and temp walls
    objectUpdates();
    // Bacteria eat, produce waste
    bacteriaUpdate();
    // Update fields, diffusion and advection
    updateFields();
    // If everyone dies, stop the simulation
    if (particles.empty()) running = false;
  }
  clock_t end = clock();
  runTime = (double)(end-start)/CLOCKS_PER_SEC;
}

double Simulator::getMark(int i) {
  return timeMarks.at(i);
}

double Simulator::getMarkSlope() {
  if (timeMarks.size()<2) return 0;
  return timeMarks.size()/(timeMarks.at(timeMarks.size()-1)-timeMarks.at(0));
}

double Simulator::getMarkDiff() {
  if (timeMarks.size()<2) return 0;
  return timeMarks.at(timeMarks.size()-1)-timeMarks.at(0);
}

vector<vect<> > Simulator::getAveProfile() {
  return aveProfile();
}

void Simulator::addStatistic(statfunc func) {
  statistics.push_back(func);
  statRec.push_back(vector<vect<>>());
}

vector<vect<> > Simulator::getStatistic(int i) {
  if (i<statistics.size()) return statRec.at(i);
  return vector<vect<> >();
}

vector<double> Simulator::getDensityXProfile() {
  vector<double> profile;
  for(int x=1; x<=secX; x++) {
    int total = 0;
    for (int y=1; y<=secY; y++)
      total += sectors[x+(secX+2)*y].size();
    profile.push_back(total);
  }
  return profile;
}

vector<double> Simulator::getDensityYProfile() {
  vector<double> profile(samplePoints,0);
  double invdy = samplePoints/(top-bottom);
  for (auto P : particles) {
    double y = P->getPosition().y-bottom;
    int index = (int)(y*invdy);
    if (0<=index || index<samplePoints) profile.at(index)++;
  }
  return profile;
}

double Simulator::aveVelocity() {
  double velocity = 0;
  int N = 0;
  for (auto P : particles)
    if (P && inBounds(P)) {
      velocity += P->getVelocity().norm();
      N++;
    }
  return N>0 ? velocity/N : -1.0;
}

double Simulator::aveVelocitySqr() {
  double vsqr = 0;
  int N = 0;
  for (auto P : particles)
    if (P && inBounds(P)) {
	vsqr += sqr(P->getVelocity());
	N++;
      }
  return N>0 ? vsqr/N : -1.0;
}

double Simulator::aveKE() {
  double KE = 0;
  int N = 0;
  for (auto P : particles) {
    if (P && inBounds(P)) {
      KE += P->getKE();
      N++;
    }
  }
  return N>0 ? KE/N : -1.0;
}

double Simulator::highestPosition() {
  double y = bottom;
  for (auto P : particles) {
    vect<> p = P->getPosition();
    if (p.y>y) y = p.y;
  }
  return y;
}

vect<> Simulator::netMomentum() {
  vect<> momentum;
  for (auto P : particles) {
    if (P && inBounds(P)) momentum += P->getMass()*P->getVelocity();
  }
  return momentum;
}

vect<> Simulator::netVelocity() {
  vect<> velocity;
  for (auto P : particles) {
    if(P && inBounds(P)) velocity += P->getVelocity();
  }
  return velocity;
}

void Simulator::setSectorDims(int sx, int sy) {
  sx = sx<1 ? 1 : sx;
  sy = sy<1 ? 1 : sy;
  // Create new sectors
  secX = sx; secY = sy;
  if (sectors) delete [] sectors;
  sectors = new list<Particle*>[(secX+2)*(secY+2)+1];
  // Add particles to the new sectors
  for (auto P : particles) {
    int sec = getSec(P->getPosition());
    sectors[sec].push_back(P);
  }
}

vector<vect<> > Simulator::getVelocityDistribution() {
  if (velocityDistribution.empty()) return vector<vect<> >();
  vector<vect<> > V;
  int i=0;
  double factor1 = maxV/vbins, factor2 = recIt*particles.size();
  for (auto v : velocityDistribution) {
    V.push_back(vect<>(i*factor1, v*factor2));
    i++;
  }
  return V;
}

vector<vect<> > Simulator::getAuxVelocityDistribution() {
  if (auxVelocityDistribution.empty()) return vector<vect<> >();
  vector<vect<> > V;
  int i=0;
  double factor1 = maxV/vbins, factor2 = recIt*particles.size();
  for (auto v : auxVelocityDistribution) {
    V.push_back(vect<>(i*factor1, v*factor2));
    i++;
  }
  return V;
}

void Simulator::setFlowV(double fv) {
  flowV = fv;
  maxV = fabs(fv)>0 ? 2.*fabs(fv) : 1.;
}

void Simulator::setDimensions(double l, double r, double b, double t) {
  if (left>=right || bottom>=top) throw BadDimChoice();
  left = l; right = r; bottom = b; top = t;
  yTop = top;
}

void Simulator::addWall(Wall* wall) {
  // Do fancier things?
  walls.push_back(wall);
}

void Simulator::addTempWall(Wall* wall, double duration) {
  tempWalls.push_back(pair<Wall*,double>(wall, duration));
}

void Simulator::addParticle(Particle* particle) {
  if (particle->isActive()) asize++;
  else psize++;
  int sec = getSec(particle->getPosition());
  sectors[sec].push_back(particle);
  particles.push_back(particle);
}

void Simulator::addWatchedParticle(Particle* p) {
  addParticle(p);
  watchlist.push_back(p);
}

vector<vect<> > Simulator::findPackedSolution(int N, double R, double left, double right, double bottom, double top) {
  vector<Particle*> parts;
  vector<Wall*> bounds;
  bounds.push_back(new Wall(vect<>(left,bottom), vect<>(left,top)));
  bounds.push_back(new Wall(vect<>(left,top), vect<>(right,top)));
  bounds.push_back(new Wall(vect<>(right,top), vect<>(right,bottom)));
  bounds.push_back(new Wall(vect<>(right,bottom), vect<>(left,bottom)));
  
  // Add particles in with small radii
  double l = left + 0.05*R, x = right - 0.05*R - l;
  double b = bottom + 0.05*R, y = top - 0.05*R - b;
  for (int i=0; i<N; i++) {
    vect<> pos = vect<>(b+x*drand48(), b+y*drand48());
    //addParticle(new Particle(pos, 0.05*R));
    parts.push_back(new Particle(pos, 0.05*R));
  }

  // Enlarge particles and thermally agitate
  int steps = 2500;
  double dr = (R-0.05*R)/(double)steps, radius = 0.05*R;;
  double mag = 5, dm = mag/(double)steps;
  for (int i=0; i<steps; i++) {
    // Particle - particle interactions
    //ppInteract();
    
    for (auto P : parts)
      for (auto Q : parts)
        if (P!=Q) P->interact(Q);

    // Thermal agitation
    mag -= dm;
    for (auto P: parts) P->applyForce(mag*randV());
    // Boundary walls
    for (auto W : bounds)
      for (auto P : parts)
	W->interact(P);
    // Other walls (so we don't put particles inside of walls)
    for (auto W : walls)
      for (auto P : parts)
        W->interact(P);
    // Adjust radius
    radius += dr;
    for (auto &P : parts) P->setRadius(radius);
    for (auto &P : parts) update(P);
  }

  // Return list of positions
  vector<vect<> > pos;
  for (auto P : parts) pos.push_back(P->getPosition());
  //particles.clear();
  return pos;
}

string Simulator::printWalls() {
  if (walls.empty()) return "{}";
  stringstream stream;
  stream << "Show[";
  for (int i=0; i<walls.size(); i++) {
    stream << "Graphics[{Thick,Red,Line[{" << walls.at(i)->getPosition() << "," << walls.at(i)->getEnd() << "}]}]";
    if (i!=walls.size()-1) stream << ",";
  }
  stream << ",PlotRange->{{0," << right << "},{0," << top << "}}]";

  string str;
  stream >> str;
  return str;
}

string Simulator::printWatchList() {
  if (recIt==0) return "{}";
  // Print out watch list record
  stringstream stream;
  string str;
  stream << "pos={";
  for (int i=0; i<watchPos.size(); i++) {
    stream << watchPos.at(i);
    if (i!=watchPos.size()-1) stream << ",";
  }
  stream << "};";
  stream >> str;
  return str;
}

string Simulator::printAnimationCommand() {
  if (recIt==0) return "{}";
  stringstream stream;
  string str1, str2;
  if (!watchlist.empty()) charRadius = (*watchlist.begin())->getRadius();
  stream << "R=" << charRadius << ";";
  stream >> str1;
  stream.clear();
  str1 += "\nframes=Table[Show[walls,Graphics[Table[Circle[pos[[j]][[i]],R],{i,1,Length[pos[[j]]]}]]],{j,1,Length[pos]}];\n";
  stream << "vid=ListAnimate[frames,AnimationRate->" << max(1.0, ceil(1.0/dispTime)*dispFactor) << "]";
  stream >> str2;
  return str1+str2;
}

string Simulator::printResource() {
  return resource.print();
}

string Simulator::printWaste() {
  return waste.print();
}

string Simulator::printFitness() {
  if (resource.getDX()==0 || resource.getDY()==0 || waste.getDX()==0 || waste.getDY()==0) return "";
  stringstream stream;
  string str;
  stream << '{';
  for (int y=1; y<secY-1; y++) {
    stream << '{';
    for (int x=1; x<secX-1; x++) {
      stream << getFitness(x,y);
      if (x!=secX-2) stream << ',';
    }
    stream << '}';
    if (y!=secY-2) stream << ',';
  }
  stream << '}';
  stream >> str;
  return str;
}

string Simulator::printResourceRec() {
  if (resourceStr.empty()) return "{}";
  string str = "{";
  resourceStr.pop_back();
  str += resourceStr;
  resourceStr += ',';
  str += "}";
  return str;
}

string Simulator::printWasteRec() {
  if (wasteStr.empty()) return "{}";
  string str = "{";
  wasteStr.pop_back();
  str += wasteStr;
  wasteStr += ',';
  str += "}";
  return str;
}

string Simulator::printFitnessRec() {
  if (fitnessStr.empty()) return "{}";
  string str = "{";
  fitnessStr.pop_back();
  str += fitnessStr;
  fitnessStr += ',';
  str += "}";
  return str;
}

void Simulator::setParticleDissipation(double d) {
  for (auto P : particles) P->setDissipation(d);
}

void Simulator::setWallDissipation(double d) {
  for (auto W : walls) W->setDissipation(d);
}

void Simulator::setParticleCoeff(double c) {
  for (auto P : particles) P->setCoeff(c);
}

void Simulator::setWallCoeff(double c) {
  for (auto W : walls) W->setCoeff(c);
}

void Simulator::setParticleDrag(double d) {
  for (auto P : particles) P->setDrag(d);
}

void Simulator::setParticleFix(bool f) {
  for (auto P : particles) P->fix(f);
}

inline void Simulator::resetVariables() {
  recIt = 0;
  time = 0;
  lastMark = startTime;
  iter = 0;
  delayTriggeredExit = false;
  lastDisp = -1e9;
  minepsilon = default_epsilon;
  runTime=0;
  running = true;
  resetStatistics();
}

inline void Simulator::initializeFields() {
  // Set physical dimensions
  waste.setBounds(left, right, bottom, top);
  resource.setBounds(left,right, bottom, top);
  // Set field wrapping
  bool wx = xLBound==xRBound && xRBound==WRAP, wy = yTBound==yBBound && yBBound==WRAP;
  resource.setWrapX(wx); resource.setWrapY(wy);
  waste.setWrapX(wx); waste.setWrapY(wy);
  buffer.setWrapX(wx); buffer.setWrapY(wy);

  //** THIS CAN BE CHANGED
  waste = 0.;
  resource = 5.;
}

inline void Simulator::calculateForces() {
  // Gravity
  if (gravity!=Zero)
    for (auto P : particles) P->applyForce(P->getMass()*gravity);
  // Flow
  if (hasDrag) {
    if (flowFunc) for (auto P : particles) P->flowForce(flowFunc);
    else for (auto P : particles) P->flowForce(Zero);
  }
  // Calculate particle-particle and particle-wall forces
  interactions();
  // Temperature causes brownian motion
  if (temperature>0) {
    for (auto P : particles) P->applyForce(temperature*randV());
  }
}

inline void Simulator::logisticUpdates() {
  // Calculate appropriate epsilon
  if (adjust_epsilon) {
    double vmax = fabs(maxVelocity());
    double amax = maxAcceleration();
    double M = max(amax, vmax);
    if (M<=0) epsilon = default_epsilon;
    else {
      epsilon = vmax>0 ? min(default_epsilon, default_epsilon/vmax) : default_epsilon;
      epsilon = amax>0 ? min(epsilon, default_epsilon/amax) : epsilon;
      epsilon = max(min_epsilon, epsilon);
    }
    if (epsilon<minepsilon) minepsilon = epsilon;
  }
  else epsilon = default_epsilon;

  // Update internal variable
  time += epsilon;
  iter++;
  if (maxIters>0 && iter>=maxIters) running = false;
  // Record data (do this before calling "update" on the particles
  if (time>startRecording && time<stopRecording && time-lastDisp>dispTime || recAllIters) record();
  // If we have set the simulation to cancel if some event does not occur for some
  // amount of time, handle that here
  if (markWatch && time>startTime && time-lastMark>delayTime) {
    running = false;
    delayTriggeredExit = true;
  }
}

inline void Simulator::objectUpdates() {
  // Update simulation
  for (auto &P : particles) update(P); // Update particles
  if (sectorize) updateSectors(); // Update sectors
  // Update temp walls
  if (!tempWalls.empty()) {
    vector<list<pair<Wall*,double> >::iterator> removal;
    for (auto w=tempWalls.begin(); w!=tempWalls.end(); ++w)
      if (w->second<time) removal.push_back(w);
    for (auto w : removal) tempWalls.erase(w);
  }
}

inline void Simulator::bacteriaUpdate() {
  // Assume that all particles are bacteria
  vector<Particle*> births; // Record bacteria to add and take away
  for (int y=1; y<secY-1; y++) 
    for (int x=1; x<secX-1; x++) {
      list<Particle*>& sect = sectors[(secX+2)*y + x+1];
      int number = sect.size();
      if (number>0) {
	// Update waste and resource fields
	double &res = resource.at(x-1,y-1), &wst = waste.at(x-1,y-1);
	wst += epsilon*secretionRate*number;
	res += epsilon*eatRate*res*number;  // eatRate = resource secretion rate
	res = res<0 ? 0 : res;
	// Calculate fitness
	//temporary values:
	double alpha1 = 1;
	double alpha2 = 1;
	double beta1 = 1;
	double csat1 = 1;
	double csat2 = 1;
	//
	double fitness = alpha1*res/(res+csat1)-alpha2*wst/(wst+csat2)-beta1*secretionRate;
	// Die if neccessary
	if (fitness<0) {
	  for (auto p=sect.begin(); p!=sect.end(); ++p) {
	    particles.remove(*p);
	    watchlist.remove(*p);
	    delete *p;
	    *p=0;
	  }
	  sect.clear();
	}
	// Reproduce if able
	else
	  for (auto P : sectors[(secX+2)*y + x+1]) {
	    Bacteria* b = dynamic_cast<Bacteria*>(P);
	    if (b->canReproduce()) {
	      double rd = b->getRepDelay();
	      double attempt = drand48();	  
	      if (attempt<fitness*rd) {
		int tries = 50; // Try to find a good spot for the baby
		vect<> pos = b->getPosition();
		double rad = b->getMaxRadius();
		for (int i=0; i<tries; i++) {
		  vect<> s = 2.1*rad*randV() + pos;
		  if (!wouldOverlap(s, rad)) {
		    Bacteria *B = new Bacteria(s, rad, 0); // No expansion time
		    B->setVelocity(b->getVelocity());
		    b->resetTimer(); // Just in case
		    births.push_back(B);
		    break;
		  }
		}
	      }
	    }
	  }
      }
    }
  for (auto P : births) addWatchedParticle(P);
}

inline void Simulator::updateFields() {
  // Diffusion of resource field
  delSqr(resource, buffer); 
  for (int y=0; y<resource.getDY(); y++)
    for (int x=0; x<resource.getDX(); x++) {
      resource.at(x,y) += epsilon*(resourceDiffusion*buffer.at(x,y) + replenish);
      resource.at(x,y) = resource.at(x,y)<0 ? 0 : resource.at(x,y);
    }
  
  // Diffusion of waste field
  delSqr(waste, buffer);
  for (int y=0;y<resource.getDY(); y++)
    for(int x=0; x<resource.getDX(); x++) {
      waste.at(x,y) += epsilon*(wasteDiffusion*buffer.at(x,y) + wasteSource);
      waste.at(x,y) = waste.at(x,y)<0 ? 0 : waste.at(x,y);
    }
}

inline double Simulator::maxVelocity() {
  double maxVsqr = -1.0;
  for (auto P : particles) {
    if (P && inBounds(P)) {
      double Vsqr = P->getVelocity().normSqr();
      if (Vsqr > maxVsqr) maxVsqr = Vsqr;
    }
  }
  return maxVsqr>0 ? sqrt(maxVsqr) : -1.0;
}

inline double Simulator::maxAcceleration() {
  double maxAsqr = -1.0;
  for (auto P : particles) {
    if (P && inBounds(P)) {
      double Asqr = P->getAcceleration().normSqr();
      if (Asqr > maxAsqr) maxAsqr = Asqr;
    }
  }
  return maxAsqr>0 ? sqrt(maxAsqr) : -1.0;
}

inline vect<> Simulator::getDisplacement(vect<> A, vect<> B) {
  // Get the correct (minimal) displacement vector pointing from B to A
  double X = A.x-B.x;
  double Y = A.y-B.y;
  if (xLBound==WRAP || xRBound==WRAP) {
    double dx = (right-left)-fabs(X);
    if (dx<fabs(X)) {
      if (X>0) X=-dx;
      else X=dx;
    }
  }
  
  if (yBBound==WRAP || yTBound==WRAP) {
    double dy =(top-bottom)-fabs(Y);
    if (dy<fabs(Y)) {
      if (Y>0) Y=-dy;
      else Y=dy;
    }
  }

  return vect<>(X,Y);
}

inline double Simulator::getFitness(int x, int y) {
  double res = resource.at(x-1,y-1), wst = waste.at(x-1,y-1);
  return res/(res+1) - wst/(wst+1);
}

inline void Simulator::interactions() {
  // Calculate particle-particle forces
  if (sectorize) ppInteract();
  else // Naive solution
    for (auto P : particles) 
      for (auto Q : particles)
	if (P!=Q) P->interact(Q);
  
  // Calculate particle-wall forces
  for (auto W : walls)
    for (auto P : particles)
      W->interact(P);
  for (auto W : tempWalls) 
    for (auto P : particles)
      W.first->interact(P);
}

inline void Simulator::update(Particle* &P) {
  // Update particle
  P->update(epsilon);
  // Keep particles in bounds
  vect<> pos = P->getPosition();

  switch(xLBound) {
  default:
  case WRAP:
    while (pos.x < left) pos.x += (right-left);
    break;
  case RANDOM:
    if (pos.x<0) {
      pos.x = right;
      pos.y = (top-2*P->getRadius())*drand48()+P->getRadius();
      int count = 0;
      while(wouldOverlap(pos, P->getRadius()) && count<10) {
	pos.y = (top-2*P->getRadius())*drand48()+P->getRadius();
	count++;
      }
      P->freeze();
    }
    break;
  case NONE: break;
  }

  switch (xRBound) {
  default:
  case WRAP:
    while (pos.x>right) pos.x-=(right-left);
    break;
  case RANDOM:
    if (pos.x>right) {
      pos.x = 0;
      pos.y = (top-2*P->getRadius())*drand48()+P->getRadius();
      int count = 0;
      while(wouldOverlap(pos, P->getRadius()) && count<10) {
        pos.y = (top-2*P->getRadius())*drand48()+P->getRadius();
	count++;
      }
      P->freeze();
    }
    break;
  case NONE: break;
  }

  switch (yBBound) {
  default:
  case WRAP:
    timeMarks.push_back(time); // Record time
    lastMark = time;
    while (pos.y<bottom) pos.y+=(top-bottom);
    break;
  case RANDOM:
    if (pos.y<0) {
      timeMarks.push_back(time); // Record time
      lastMark = time;
      pos.y = yTop+4*P->getRadius()*drand48();
      pos.x = (right-2*P->getRadius())*drand48()+P->getRadius();
      int count = 0;
      while(wouldOverlap(pos, P->getRadius()) && count<10) {
	pos.y = yTop+4*P->getRadius()*drand48();
	pos.x = (right-2*P->getRadius())*drand48()+P->getRadius();
	count++;
      }
      P->freeze();
      break;
    }
  case NONE: break;
  }

  switch (yTBound) {
  default:
  case WRAP:
    while (pos.y>top) pos.y-=(top-bottom);
    break;
  case RANDOM:
    if (pos.y>top) {
      pos.y = 0;
      pos.x = (right-2*P->getRadius())*drand48()+P->getRadius();
      int count = 0;
      while(wouldOverlap(pos, P->getRadius()) && count<10) {
	pos.x = (right-2*P->getRadius())*drand48()+P->getRadius();
        count++;
      }
      P->freeze();
    }
    break;
  case NONE: break;
  }

  // Update the particle's position
  P->getPosition() = pos;
}

inline void Simulator::record() {
  // Record positions
  watchPos.push_back(vector<vect<> >());
  for (auto P : watchlist)
    watchPos.at(recIt).push_back(P->getPosition());

  // Record statistics
  for (int i=0; i<statistics.size(); i++)
  statRec.at(i).push_back(vect<>(time, statistics.at(i)(particles)));
  
  // Record density profile //** Temporary? Find a more general way to do this?
  profiles.push_back(getDensityYProfile());

  // Record velocity distribution
  for (auto P : particles) {
    double vel = sqrt(sqr(P->getVelocity()));
    double fvel = sqrt(sqr(flowFunc(P->getPosition())));
    int B = (int)(vel/maxV*vbins);
    int Bf = fvel>0 ? (int)(vel/fvel/maxF*vbins) : vbins-1;
    B = B>=vbins ? vbins-1 : B;
    Bf = Bf>=vbins ? vbins-1 : Bf;

    try {
    velocityDistribution.at(B)++;
    auxVelocityDistribution.at(Bf)++;
    }
    catch(...) {

      cout << vel << " " << maxV << " " << fvel << endl;

      cout << B << " " << Bf << " " << vbins << endl;
      throw;
    }
  }

  // Record fields
  if (recFields) {
    resourceStr += (printResource()+',');
    wasteStr += (printWaste()+',');
    fitnessStr += (printFitness()+',');
  }

  // Update time
  lastDisp = time;
  recIt++;
}

inline bool Simulator::inBounds(Particle* P) {
  vect<> pos = P->getPosition();
  double radius = P->getRadius();
  if (pos.x+radius<0 || pos.x-radius>right) return false;
  if (pos.y+radius<0 || pos.y-radius>top) return false;
  return true;
}

void Simulator::addParticles(int N, double R, double var, double lft, double rght, double bttm, double tp, PType type, double vmax, bool watched, vect<> bias) {
  bool C = true;
  int maxFail = 250;
  int count = 0, failed = 0;
  double diffX = rght - lft - 2*R;
  double diffY = tp - bttm - 2*R;
  while (count<N && C) {
    vect<> pos(lft+diffX*drand48()+R, bttm+diffY*drand48()+R);
    if (!wouldOverlap(pos, R)) {
      double rad = R*(1+var*drand48());
      Particle *P;
      switch (type) {
      default:
      case PASSIVE: {
	P = new Particle(pos, rad);
	break;
      }
      case RTSPHERE: {
	P = new RTSphere(pos, rad, bias);
	break;
      }
      case BACTERIA: {
	P = new Bacteria(pos, rad);
	break;
      }
      }
      if (watched) addWatchedParticle(P);
      else addParticle(P);
      if (vmax > 0) P->setVelocity(vmax*randV());
      count++;
      failed = 0;
    }
    else {
      failed++;
      if (failed > maxFail) C = false; // To many failed tries
    }
  }
}

void Simulator::addNWParticles(int N, double R, double var, double lft, double rght, double bttm, double tp, PType type, double vmax) {
  addParticles(N, R, var, lft, rght, bttm, tp, type, vmax, false);
}

void Simulator::addRTSpheres(int N, double R, double var, double lft, double rght, double bttm, double tp, vect<> bias) {
  addParticles(N, R, var, lft, rght, bttm, tp, RTSPHERE, -1, true, bias);
}

void Simulator::resetStatistics() {
  for (auto &vec : statRec) vec.clear();
}

inline void Simulator::updateSectors() {
  for (int i=0; i<(secX+2)*(secY+2)+1; i++) {
    vector<list<Particle*>::iterator> remove;
    for (auto p=sectors[i].begin(); p!=sectors[i].end(); ++p) {
      int sec = getSec((*p)->getPosition());
      if (sec != i) { // In the wrong sector
	remove.push_back(p);
	sectors[sec].push_back(*p);
      }
    }
    // Remove particles that moved
    for (auto P : remove) sectors[i].erase(P);
  }
}

inline void Simulator::ppInteract() {
  for (int y=1; y<secY+1; y++)
    for (int x=1; x<secX+1; x++)
      // Check in surrounding sectors
      for (auto P : sectors[y*(secX+2)+x]) {
	// Check surrounding sectors
	for (int j=y-1; j<=y+1; j++) {
	  int sy = j;
	  if ((yBBound==WRAP || yTBound==WRAP) && j==0) sy=secY;
	  else if ((yBBound==WRAP ||yTBound==WRAP) && j==secY+1) sy=1;
	  for (int i=x-1; i<=x+1; i++) {
	    int sx = i;
	    if ((xLBound==WRAP || xRBound==WRAP) && i==0) sx=secX;
	    else if ((xLBound==WRAP || xRBound==WRAP) && i==secX+1) sx=1;	    
	    for (auto Q : sectors[sy*(secX+2)+sx])
	      if (P!=Q) {
		vect<> disp = getDisplacement(Q->getPosition(), P->getPosition());
		P->interact(Q, disp);
	      }
	  }
	}
      }
  // Have to try to interact everything in the special sector with everything else
  if (ssecInteract) {
    for (auto P : sectors[(secX+2)*(secY+2)])
      for (auto Q : particles)
	if (P!=Q) {
	  vect<> disp = getDisplacement(Q->getPosition(), P->getPosition());
	  P->interact(Q);
	}
  }
}

inline int Simulator::getSec(vect<> pos) {
  int X = static_cast<int>((pos.x-left)/(right-left)*secX);
  int Y = static_cast<int>((pos.y-bottom)/(top-bottom)*secY);  
  
  // If out of bounds, put in the special sector
  if (X<0 || Y<0 || X>secX || Y>secY) return (secX+2)*(secY+2);
  
  return (X+1)+(secX+2)*(Y+1);
}

void Simulator::discard() {
  psize = asize = 0;
  for (int i=0; i<(secX+2)*(secY+2); i++) sectors[i].clear();
  for (auto P : particles) 
    if (P) {
      delete P;
      P = 0;
    }
  particles.clear();
  watchlist.clear();
  watchPos.clear();
  for (auto W : walls) 
    if (W) {
      delete W;
      W = 0;
    }
  walls.clear();
  for (auto W : tempWalls) 
    if (W.first) {
      delete W.first;
      W.first = 0;
    }
  tempWalls.clear();
  // Clear time marks and statistics
  timeMarks.clear();
  for (auto V : statRec) V.clear();
}

inline vector<vect<> > Simulator::aveProfile() {
  if (profiles.empty()) return vector<vect<> >();
  vector<vect<> > ave = vector<vect<> >(samplePoints, vect<>());
  for (int i=0; i<samplePoints; i++) ave.at(i).x = (double)i/samplePoints;
  for (auto p : profiles)
    for (int i=0; i<samplePoints; i++) ave.at(i).y += p.at(i);

  double factor = (double)samplePoints/(profiles.size()*(right-left));
  for (auto &d : ave) d.y *= factor;
  return ave;
}
