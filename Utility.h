//
//  Utility.h
//  Predictive1
//
//  Created by Nathaniel Rupprecht on 1/25/16.
//  Copyright (c) 2016 Nathaniel Rupprecht. All rights reserved.
//

#ifndef Predictive1_Utility_h
#define Predictive1_Utility_h

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <math.h>
#include <iostream>
#include <string>
#include <sstream>

using std::vector;
using std::pair;
using std::cout;
using std::endl;
using std::string;
using std::stringstream;

const double PI = 3.14159265;

/// Random number function
inline double getRand() { return drand48(); }

inline double limit_prec(double x) { return fabs(x)<1e-4 ? 0 : x; }

/// Forward declaration for Vector Field
struct VectorField;

/// Helper two argument min function
template<typename T> inline T min(T a, T b) {
  return a<b ? a : b;
}

template<typename T> inline T max(T a, T b) {
  return a<b ? b : a;
}

/// Helper function two argument absmin function
template<typename T> inline T absmin(T a, T b) {
  return fabs(a)<fabs(b) ? a : b;
}

/// Helper three argument min function
template<typename T> inline T min(T a, T b, T c) {
  return min(min(a, b), c);
}

/// Helper three argument absmin function
template<typename T> inline T absmin(T a, T b, T c) {
  return absmin(absmin(a, b), c);
}

/// Helper squaring function
template<typename T> inline T sqr(T x) { return x*x; }

/// A simple 2-D vector struct
template<typename T=double> struct vect {
vect(T x, T y) : x(x), y(y) {};
vect() : x(T(0)), y(T(0)) {};
vect(const vect<T>& V) : x(V.x), y(V.y) {};
    
  T norm() { return sqrtf(x*x + y*y); }
  T normSqr() { return x*x + y*y; }
    
  void normalize() {
    T nrm = norm();
    T n = nrm!=0 ? 1.f/nrm : 1;
    x *= n; y *= n;
  }
    
  vect<T>& operator=(const vect<T>& V) {
    x = V.x; y = V.y;
    return *this;
  }
    
    bool operator==(const vect<T>& V) {
      return x==V.x && y==V.y;
    }
    
  friend std::ostream& operator<<(std::ostream& os, const vect& v) {
    os << "{" << limit_prec(v.x) << "," << limit_prec(v.y) << "}";
    return os;
  }
    
  T operator*(const vect<T>& B) {
    return x*B.x + y*B.y;
  }
    
  vect<T> operator+(const vect<T>& B) {
    return vect<T>(x+B.x, y+B.y);
  }

  vect<T> operator-(const vect<T>& B) {
    return vect<T>(x-B.x, y-B.y);
  }
    
  vect<T> operator+=(const vect<T>& B) {
    x += B.x; y += B.y;
    return *this;
  }

  vect<T> operator-=(const vect<T>& B) {
    x -= B.x; y -= B.y;
    return *this;
  }
    
  vect<T> operator*=(const T num) {
    x *= num;
    y *= num;
    return *this;
  }
    
  friend vect<T> operator*(const T num, const vect<T>& V) {
    return vect<T>(V.x*num, V.y*num);
  }
    
  static vect<> rand() {
    return vect<>(0.5-drand48(), 0.5-drand48());
  }
    
  /// The actual data
  T x, y;
};

template<typename T> vect<T> normalize(vect<T> V) {
  V.normalize();
  return V;
}

template<typename T> T sqr(vect<T> V) { return V*V; }

template<typename T> inline vect<T> normalV(T x, T y) {
  T ns = x*x + y*y;
  T n = sqrt(ns);
  return n>0 ? vect<T> (x/n, y/n) : vect<T>();
}

template<typename T> inline T distSqr(const vect<T>& A, const vect<T>& B) {
  return sqr(A.x-B.x)+sqr(A.y-B.y);
}

inline vect<> randV() {
  float a = drand48();
  return vect<>(sinf(2*PI*a), cosf(2*PI*a));
}

template<typename T> inline std::ostream& operator<<(std::ostream& out, vector<T> lst) {
  out << "{";
  for (int i=0; i<lst.size(); i++) {
    out << lst.at(i);
    if (i!=lst.size()-1) out << ",";
  }
  out << "}";
  return out;
}

/*
inline std::ostream& operator<<(std::ostream& out, vector<vect<>> lst) {
  out << "{";
  for (int i=0; i<lst.size(); i++) {
    out << lst.at(i);
    if (i!=lst.size()-1) out << ",";
  }
  out << "}";
  return out;
}
*/

// A useful typedef
typedef pair<vect<float>, bool> vtype;
typedef pair<int,int> ipair;

/// The Agent structure
struct Agent {
  Agent(vect<> pos)
  : position(pos), initPos(pos) {};
  Agent(vect<> pos, ipair sec, int ID)
  : position(pos), initPos(pos), sector(sec), ID(ID) {};
    
  vect<> position;    // The "current" position of the agent
  vect<> initPos;     // The initial position the agent started at
  ipair sector;
  int ID;
  
  bool operator==(const Agent A) const { return ID==A.ID; }
    
  void reset() { position = initPos; }
    
  float __pad;
};

#endif