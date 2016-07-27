/*
 val3dity - Copyright (c) 2011-2016, Hugo Ledoux.  All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
     * Neither the name of the authors nor the
       names of its contributors may be used to endorse or promote products
       derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL HUGO LEDOUX BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
*/

#include "input.h"
#include "Solid.h"
#include "Shell.h"
#include <CGAL/Nef_polyhedron_3.h>
#include <CGAL/IO/Polyhedron_iostream.h>
#include <CGAL/IO/Nef_polyhedron_iostream_3.h>
#include <CGAL/Polyhedron_copy_3.h>

typedef CGAL::Exact_predicates_exact_constructions_kernel   KE;
typedef CGAL::Polyhedron_3<KE>                              CgalPolyhedronE;
typedef CGAL::Nef_polyhedron_3<KE>                          Nef_polyhedron;

typedef CGAL::Polyhedron_copy_3<CgalPolyhedron, CgalPolyhedronE::HalfedgeDS> Polyhedron_convert; 

//-- to keep track of all gml:Solids in a GML file
int Solid::_counter = 0;

Solid::Solid()
{
  _id = std::to_string(_counter);
  _counter++;
  _is_valid = -1;
}


Solid::Solid(Shell* sh)
{
  _shells.push_back(sh);
  _id = std::to_string(_counter);
  _counter++;
}

Solid::~Solid()
{
  // TODO: destructor for Solid, memory-management jongens
}

Shell* Solid::get_oshell()
{
  return _shells[0];
}


void Solid::set_oshell(Shell* sh)
{
  if (_shells.empty())
    _shells.push_back(sh);
  else
    _shells[0] = sh;
}


const vector<Shell*>& Solid::get_shells()
{
  return _shells;
}


void Solid::add_ishell(Shell* sh)
{
  _shells.push_back(sh);
}


bool Solid::is_valid()
{
  if ( (_is_valid > 0) && (this->is_empty() == false) )
    return true;
  else
    return false;
}


bool Solid::is_empty()
{
  for (auto& sh : _shells)
  {
    if (sh->is_empty() == true)
      return true;
  }
  return false;
}


void Solid::translate_vertices()
{
  double minx = 9e10;
  double miny = 9e10;
  for (auto& sh : _shells)
  {
    double tx, ty;
    sh->get_min_bbox(tx, ty);
    if (tx < minx)
      minx = tx;
    if (ty < miny)
      miny = ty;
  }
  for (auto& sh : _shells)
    sh->translate_vertices(minx, miny);
}


bool Solid::validate(Primitive3D prim, double tol_planarity_d2p, double tol_planarity_normals)
{
  bool isValid = true;
  if (this->is_empty() == true)
  {
    this->add_error(902, -1, -1, "probably error while parsing GML input");
    return false;
  }
  for (auto& sh : _shells)
  {
    if (sh->validate(prim, tol_planarity_d2p, tol_planarity_normals) == false) 
      isValid = false;
  }
  if (isValid == true)
  {
    if (validate_solid_with_nef() == false)
      isValid = false;
  }
  _is_valid = isValid;
  return isValid;
}


std::set<int> Solid::get_unique_error_codes()
{
  std::set<int> errs;
  for (auto& sh : _shells)
  {
    std::set<int> errsh = sh->get_unique_error_codes();
    errs.insert(errsh.begin(), errsh.end());
  }
  return errs;
}

std::string Solid::get_poly_representation()
{
  std::ostringstream s;
  for (auto& sh : _shells)
  {
    s << sh->get_poly_representation() << std::endl;
  }
  return s.str();
}

std::string Solid::get_report_xml()
{
  std::stringstream ss;
  ss << "\t<Primitive>" << std::endl;
  ss << "\t\t<id>" << this->_id << "</id>" << std::endl;
  ss << "\t\t<numbershells>" << (this->num_ishells() + 1) << "</numbershells>" << std::endl;
  ss << "\t\t<numberfaces>" << this->num_faces() << "</numberfaces>" << std::endl;
  ss << "\t\t<numbervertices>" << this->num_vertices() << "</numbervertices>" << std::endl;
  for (auto& err : _errors)
  {
    for (auto& e : _errors[std::get<0>(err)])
    {
      ss << "\t\t<Error>" << std::endl;
      ss << "\t\t\t<code>" << std::get<0>(err) << "</code>" << std::endl;
      ss << "\t\t\t<type>" << errorcode2description(std::get<0>(err)) << "</type>" << std::endl;
      ss << "\t\t\t<shell>" << std::get<0>(e) << ";" << std::get<1>(e) << "</shell>" << std::endl;
      ss << "\t\t\t<info>" << std::get<2>(e) << "</info>" << std::endl;
      ss << "\t\t</Error>" << std::endl;
    }
  }
  for (auto& sh : _shells)
  {
    ss << sh->get_report_xml();
  }
  ss << "\t</Primitive>" << std::endl;
  return ss.str();
}

std::string Solid::get_report_text()
{
  std::stringstream ss;
  ss << "===== Primitive " << this->_id << " =====" << std::endl;
  for (auto& err : _errors)
  {
    for (auto& e : _errors[std::get<0>(err)])
    {
      ss << "\t" << std::get<0>(err) << " -- " << errorcode2description(std::get<0>(err)) << std::endl;
      ss << "\t\tShells: " << std::get<0>(e) << ";" << std::get<1>(e) << std::endl;
      // ss << "\t\tFace: "  << std::get<0>(e) << std::endl;
      ss << "\t\tInfo: "  << std::get<2>(e) << std::endl;
    }
  }
  for (auto& sh : _shells)
  {
    ss << sh->get_report_text();
  }
  if (this->is_valid() == true)
    ss << "\tVALID" << std::endl;
  return ss.str();
}


int Solid::num_ishells()
{
  return (_shells.size() - 1);
}

int Solid::num_faces()
{
  int total = 0;
  for (auto& sh : _shells)
    total += sh->number_faces();
  return total;
}

int Solid::num_vertices()
{
  int total = 0;
  for (auto& sh : _shells)
    total += sh->number_vertices();
  return total;
}

std::string Solid::get_id()
{
  return _id;
}


void Solid::set_id(std::string id)
{
  _id = id;
}


void Solid::add_error(int code, int shell1, int shell2, std::string info)
{
  std::tuple<int, int, std::string> a(shell1, shell2, info);
  _errors[code].push_back(a);
  std::clog << "\tERROR " << code << ": " << errorcode2description(code);
  std::clog << " (shells: #" << shell1 << " & #" << shell2 << ")" << std::endl;
  if (info.empty() == false)
    std::clog << "\t[" << info << "]" << std::endl;
}


// bool validate_solid_with_nef(vector<CgalPolyhedron*> &polyhedra, cbf cb)
bool Solid::validate_solid_with_nef()
{
  if (this->num_ishells() == 0)
    return true;
    
  bool isValid = true;
  std::stringstream st;
  std::clog << "----------" << std::endl << "--Inspection interactions between the " << (this->num_ishells() + 1) << " shells" << std::endl;
  vector<Nef_polyhedron> nefs;
  for (auto& sh : this->get_shells())
  {
    //-- convert to an EPEC Polyhedron so that convertion to Nef is possible
    CgalPolyhedronE pe;
    Polyhedron_convert polyhedron_converter(*(sh->get_cgal_polyhedron()));
    pe.delegate(polyhedron_converter);
    Nef_polyhedron onef(pe);
    nefs.push_back(onef);
  }

  bool success = true;
  
  //-- test axiom #1 from the paper
  Nef_polyhedron nef;
  for (int i = 1; i < nefs.size(); i++) 
  {
    nef = !nefs[0] * nefs[i];
    if (nef.is_empty() == false)
    {
      success = false;
      std::cout << ">>>>>AXIOM 1" << std::endl;
    }
  }

  //-- test axiom #2 from the paper
  nef.clear();
  for (int i = 1; i < nefs.size(); i++) 
  {
    for (int j = (i + 1); j < nefs.size(); j++) 
    {
      nef = nefs[i] * nefs[j];
      if (nef.number_of_volumes() > 0)
      {
        success = false;
        std::cout << ">>>>>AXIOM 2" << std::endl;
      }
    }
  }
  
  //-- test axiom #3 from the paper
  nef.clear();
  nef += nefs[0];
  int numvol = 2;
  for (int i = 1; i < nefs.size(); i++) 
  {
    nef = nef - nefs[i];
    nef.regularization();
    numvol++;
    if (nef.number_of_volumes() != numvol)
    {
      success = false;
      std::cout << ">>>>>AXIOM 3" << std::endl;
      break;
    }
  }




//   if (success == false) //-- the Nef is not valid, pairwise testing to see what's wrong
//   {
//     isValid = false;
// //-- start with oshell<-->ishells
//     nefsIt = nefs.begin();
//     nefsIt++;
//     int no = 1;
//     for ( ; nefsIt != nefs.end(); nefsIt++) 
//     {
//       nef.clear();
//       nef += *(nefs.begin());
//       nef -= *nefsIt;
//       nef.regularization(); 
//       if (nef.number_of_volumes() != 3)
//       {
//         if (nef.number_of_volumes() > 3)
//         {
//           //-- check if ishell is a subset of oshell
//           if ((*nefsIt <= nefs[0]) == true)
//             this->add_error(404, 0, no, "");
//           else
//           {
//             this->add_error(402, 0, no, "");
//             this->add_error(404, 0, no, "");
//           }
//         }
//         else //-- nef.number_of_volumes() < 3
//         {
//           //-- perform union
//           nef.clear();
//           nef += *(nefs.begin());
//           nef += *nefsIt;
//           nef.regularization();
//           if (nef.number_of_volumes() == 3)
//             this->add_error(403, -1, -1, "");
//           else
//           {
//             if ((*nefsIt <= nefs[0]) == true)
//               this->add_error(401, 0, no, "");
//             else
//             {
//               nef.clear();
//               nef = nefs[0].intersection(nefsIt->interior());
//               nef.regularization();
//               if (nef.is_empty() == true)
//               {
//                 this->add_error(401, 0, no, "");
//                 this->add_error(403, 0, no, "");
//               }
//               else
//                 this->add_error(402, 0, no, "");
//             }
//           }
//         }
//       }
//     no++;
//     }

// //-- then check ishell<-->ishell interactions
//     nefsIt = nefs.begin();
//     nefsIt++;
//     vector<Nef_polyhedron>::iterator nefsIt2;
//     no = 1;
//     int no2;
//     for ( ; nefsIt != nefs.end(); nefsIt++)
//     {
//       nefsIt2 = nefsIt;
//       nefsIt2++;
//       no2 = no + 1;
//       for ( ; nefsIt2 != nefs.end(); nefsIt2++)
//       {
//         nef.clear();
//         nef += *nefsIt;
//         nef += *nefsIt2;
//         nef.regularization();
//         if (nef.number_of_volumes() > 3)
//           this->add_error(402, no, no2, "Both shells completely overlap");
//         else if (nef.number_of_volumes() < 3)
//         {
//           //-- either they are face adjacent or overlap
//           nef.clear();
//           nef = nefsIt->interior();
//           nef = nef.intersection(nefsIt2->interior());
//           nef.regularization();
//           if (nef.is_empty() == true)
//             this->add_error(401, no, no2, "");
//           else
//             this->add_error(402, no, no2, "");
//         }
//         no2++;
//       }
//       no++;
//     }
//   }
  return isValid;
}