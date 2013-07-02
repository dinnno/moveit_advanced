/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2013, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Acorn Pooley */

#include <mesh_core/mesh.h>
#include <mesh_core/geom.h>
#include <console_bridge/console.h>

mesh_core::Mesh::Mesh(double epsilon)
  : have_degenerate_edges_(false)
  , number_of_submeshes_(-1)
  , adjacent_tris_valid_(false)
{
  setEpsilon(epsilon);
}

void mesh_core::Mesh::clear()
{
  verts_.clear();
  tris_.clear();
}

void mesh_core::Mesh::reserve(int ntris, int nverts)
{
  tris_.reserve(tris_.size() + ntris);
  if (nverts == 0)
    nverts = ntris * 3;
  verts_.reserve(verts_.size() + nverts);
}

void mesh_core::Mesh::add(
      int a,
      int b,
      int c)
{
  if (a == b || a == c || b == c)
    return;

  int tri_idx = tris_.size();
  tris_.resize(tri_idx + 1);
  Triangle &t = tris_.back();
  t.verts_[0] = a;
  t.verts_[1] = b;
  t.verts_[2] = c;
  t.submesh_ = -1;
  t.mark_ = 0;

  number_of_submeshes_ = -1; // need to recalculate
  adjacent_tris_valid_ = false;

  // find 3 edges
  for (int dir = 0; dir < 3 ; ++dir)
  {
    TriEdge& te = t.edges_[dir];
    te.adjacent_tri_ = -1;
    te.adjacent_tri_back_dir_ = -1;
    te.edge_idx_ = addEdge(t.verts_[dir], t.verts_[(dir+1)%3]);

    Edge& edge = edges_[te.edge_idx_];
    te.edge_tri_idx_ = edge.tris_.size();
    edge.tris_.resize(te.edge_tri_idx_ + 1);

    EdgeTri& et = edge.tris_[te.edge_tri_idx_];
    et.tri_idx_ = tri_idx;
    et.tri_dir_ = dir;

    // have more than 2 tris per edge?
    if (edge.tris_.size() > 2)
      have_degenerate_edges_ = true;
  }

  assertValidTri_PreAdjacentValid(t, "add tri");
}

void mesh_core::Mesh::add(
      const Eigen::Vector3d& a,
      const Eigen::Vector3d& b,
      const Eigen::Vector3d& c)
{
  verts_.reserve(verts_.size()+3);
  int ai = addVertex(a);
  int bi = addVertex(b);
  int ci = addVertex(c);
  add(ai, bi, ci);
}


void mesh_core::Mesh::add(
      double *a,
      double *b,
      double *c)
{
  Eigen::Vector3d av(a[0], a[1], a[2]);
  Eigen::Vector3d bv(b[0], b[1], b[2]);
  Eigen::Vector3d cv(a[0], c[1], c[2]);
  add(av, bv, cv);
}

void mesh_core::Mesh::add(
      const EigenSTL::vector_Vector3d& verts,
      int ntris,
      int *tris)
{
  tris_.reserve(tris_.size() + ntris);
  verts_.reserve(verts_.size() + (ntris * 3));
  for (int i = 0; i < ntris ; ++i)
  {
    add(verts[tris[i*3+0]],
        verts[tris[i*3+1]],
        verts[tris[i*3+2]]);
  }
}

void mesh_core::Mesh::add(
      double *verts,
      int ntris,
      int *tris)
{
  tris_.reserve(tris_.size() + ntris);
  verts_.reserve(verts_.size() + (ntris * 3));
  for (int i = 0; i < ntris ; ++i)
  {
    add(&verts[tris[i*3+0] * 3],
        &verts[tris[i*3+1] * 3],
        &verts[tris[i*3+2] * 3]);
  }
}


void mesh_core::Mesh::findSubmeshes()
{
  setAdjacentTriangles();

  if (number_of_submeshes_ > 0)
    return;

  submesh_tris_.clear();

  std::vector<Triangle>::iterator it = tris_.begin();
  std::vector<Triangle>::iterator end = tris_.end();
  for ( ; it != end ; ++it)
  {
    it->submesh_ = -1;
  }

  number_of_submeshes_ = 0;

  for (it = tris_.begin() ; it != end ; ++it)
  {
    if (it->submesh_ == -1)
    {
      submesh_tris_.resize(number_of_submeshes_+1);
      submesh_tris_[number_of_submeshes_] = it - tris_.begin();

      assignSubmesh(*it, number_of_submeshes_);
      number_of_submeshes_++;
    }
  }
}

void mesh_core::Mesh::assignSubmesh(Triangle& t, int submesh)
{
  t.submesh_ = submesh;
  for (int dir = 0 ; dir < 3 ; ++dir)
  {
    int tadj_idx = t.edges_[dir].adjacent_tri_;
    if (tadj_idx < 0)
      continue;
    Triangle& tadj = tris_[tadj_idx];

    if (tadj.submesh_ != submesh)
      assignSubmesh(tadj, submesh);
  }
}

void mesh_core::Mesh::setEpsilon(double epsilon)
{
  epsilon_ = std::abs(epsilon);
  epsilon_squared_ = epsilon_ * epsilon_;
  if (epsilon_squared_ < std::numeric_limits<double>::epsilon())
    epsilon_squared_ = std::numeric_limits<double>::epsilon();
}

bool mesh_core::Mesh::Triangle::operator==(const Triangle& b) const
{
  for (int i = 0 ; i < 3 ; i++)
  {
    if (verts_[0] != b.verts_[i])
      continue;
    if (verts_[1] != b.verts_[(i+1)%3])
      continue;
    if (verts_[2] != b.verts_[(i+2)%3])
      continue;
    return true;
  }
  return false;
}

int mesh_core::Mesh::addVertex(const Eigen::Vector3d& a)
{
  EigenSTL::vector_Vector3d::const_iterator it = verts_.begin();
  EigenSTL::vector_Vector3d::const_iterator end = verts_.end();
  for ( ; it != end ; ++it)
  {
    if ((*it - a).squaredNorm() < epsilon_squared_)
      return it - verts_.begin();
  }
  verts_.push_back(a);

  vert_info_.resize(verts_.size());
  vert_info_[verts_.size() - 1].edges_.reserve(8);

  return verts_.size() - 1;
}

int mesh_core::Mesh::addEdge(int vertidx_a, int vertidx_b)
{
  assert(vertidx_a != vertidx_b);

  if (vertidx_a > vertidx_b)
    std::swap(vertidx_a, vertidx_b);

  std::pair<int,int> key(vertidx_a, vertidx_b);

  std::map<std::pair<int,int>,int>::const_iterator it = edge_map_.find(key);
  if (it != edge_map_.end())
    return it->second;

  int edge_idx = edges_.size();
  edges_.resize(edge_idx + 1);
  Edge &e = edges_.back();
  e.verts_[0] = vertidx_a;
  e.verts_[1] = vertidx_b;
  e.tris_.reserve(2);

  edge_map_[key] = edge_idx;

  // add edge to per-vertex list of edges
  for (int i = 0 ; i < 2 ; ++i)
  {
    vert_info_[e.verts_[i]].edges_.push_back(edge_idx);
  }

  return edge_idx;
}


void mesh_core::Mesh::clearMark(int value)
{
  std::vector<Triangle>::iterator it = tris_.begin();
  std::vector<Triangle>::iterator end = tris_.end();
  for ( ; it != end ; ++it)
  {
    assertValidTri(*it, "clearMark");
    it->mark_ = value;
  }
}

void mesh_core::Mesh::setAdjacentTriangles()
{
  if (adjacent_tris_valid_)
    return;

  int tri_cnt = tris_.size();
  for (int tri_idx = 0 ; tri_idx != tri_cnt ; ++tri_idx)
  {
    Triangle& tri = tris_[tri_idx];
    for (int dir = 0 ; dir < 3 ; ++dir)
    {
      TriEdge& te = tri.edges_[dir];
      te.adjacent_tri_ = -1;
      te.adjacent_tri_back_dir_ = -1;

      int edge_idx = te.edge_idx_;
      Edge& edge = edges_[edge_idx];

      ACORN_ASSERT(edge.tris_.size() > 0);
      if (edge.tris_.size() == 2)
      {
        if (edge.tris_[0].tri_idx_ == tri_idx)
        {
          te.adjacent_tri_ = edge.tris_[1].tri_idx_;
        }
        else
        {
          ACORN_ASSERT(edge.tris_[1].tri_idx_ == tri_idx);
          te.adjacent_tri_ = edge.tris_[0].tri_idx_;
        }

        Triangle& adj = tris_[te.adjacent_tri_];

        for (int back_dir = 0 ;; ++back_dir)
        {
          ACORN_ASSERT(back_dir < 3);
          if (back_dir >= 3)
            break;

          TriEdge& adj_te = adj.edges_[back_dir];
          if (adj_te.edge_idx_ == edge_idx)
          {
            te.adjacent_tri_back_dir_ = back_dir;
            break;
          }
        }

      }
      else if (edge.tris_.size() > 2)
      {
        te.adjacent_tri_ = -2;
      }
    }
  }
  adjacent_tris_valid_ = true;
}

// make all windings point the same way
void mesh_core::Mesh::fixWindings()
{
  int mark = 0;

  findSubmeshes();
  clearMark(mark);


  for (int i=0; i<number_of_submeshes_; ++i)
  {
    int tri_cnt = 0;
    int flip_cnt = 0;
    fixWindings(
              tris_[submesh_tris_[i]],
              false,
              ++mark,
              tri_cnt,
              flip_cnt);
    if (flip_cnt > tri_cnt/2)
    {
      // guessed wrong - flip them all back the opposite way
      fixWindings(
              tris_[submesh_tris_[i]],
              true,
              ++mark,
              tri_cnt,
              flip_cnt);
    }
  }
}

bool mesh_core::Mesh::isWindingSame(Triangle& tri, int dir)
{
  int adj_idx = tri.edges_[dir].adjacent_tri_;
  if (adj_idx < 0)
    return false;
  Triangle& adj = tris_[adj_idx];

  int back_dir = tri.edges_[dir].adjacent_tri_back_dir_;
  return tri.verts_[dir] != adj.verts_[back_dir];
}

namespace {
struct FlipInfo
{
  mesh_core::Mesh::Triangle* tri;
  bool flip;
};
}

// if possible, make all windings point the same way for all triangles in this
// tris submesh.
// Should only be called from fixWindings().
// Returns the total number of triangles in the submesh and the number of triangles flipped.
// If flip_this_tri is true, the initial triangle's winding is flipped.
void mesh_core::Mesh::fixWindings(
      Triangle& first_tri,
      bool flip_first_tri,
      int mark,
      int& tri_cnt,
      int& flip_cnt)
{
  assertValidTri(first_tri, "fixWindings1");

  std::vector<FlipInfo> list;
  list.resize(tris_.size());

  list[0].tri = &first_tri;
  list[0].flip = flip_first_tri;
  first_tri.mark_ = mark;

  FlipInfo *tail = &list[0];
  FlipInfo *head = &list[1];
  
  // go through all tris in the same submesh and make them consistent with the
  // first_tri
  do
  {
    FlipInfo &info = *tail++;
    Triangle& tri = *info.tri;
    tri_cnt++;

    assertValidTri(tri, "fixWindings2");

    if (info.flip)
    {
      flipWinding(*info.tri);
      flip_cnt++;
    }
    
    for (int dir = 0 ; dir < 3 ; ++dir)
    {
      int adj_idx = tri.edges_[dir].adjacent_tri_;
      if (adj_idx < 0)
        continue;
      Triangle& adj = tris_[adj_idx];

      assertValidTri(adj, "fixWindings3");

      // visit each triangle only once
      if (adj.mark_ == mark)
        continue;
      adj.mark_ = mark;

      head->tri = &adj;
      head->flip = !isWindingSame(tri, dir);

      head++;

      if (head - &list[0] > list.size())
      {
        logError("fixWindings OVERFLOWED!");
        return;
      }
    }
  }
  while (tail != head);
}

// flip the winding order by swapping vertex 1 and 2
void mesh_core::Mesh::flipWinding(Triangle& tri)
{
  ACORN_ASSERT_TRI(tri);
  assertValidTri(tri, "flipWinding1");

  std::swap(tri.verts_[1], tri.verts_[2]);
  std::swap(tri.edges_[0], tri.edges_[2]);

  // fixup edge 0 and 2 which got swapped
  for (int dir = 0 ; dir < 3 ; dir++)
  {
    if (dir == 1)
      continue;

    TriEdge& te = tri.edges_[dir];
    Edge& edge = edges_[te.edge_idx_];
    EdgeTri& et = edge.tris_[te.edge_tri_idx_];
    et.tri_dir_ = dir;

    if (adjacent_tris_valid_ && te.adjacent_tri_ >= 0)
    {
      Triangle& adj = tris_[te.adjacent_tri_];
      TriEdge& adj_te = adj.edges_[te.adjacent_tri_back_dir_];
      adj_te.adjacent_tri_back_dir_ = dir;
    }
  }

  assertValidTri(tri, "flipWinding2");
}

void mesh_core::Mesh::assertValidTri(
      const Triangle& tri,
      const char *msg) const
{
  ACORN_ASSERT(adjacent_tris_valid_);
  ACORN_ASSERT(number_of_submeshes_ > 0);
  assertValidTri_PreAdjacentValid(tri, msg);
}

void mesh_core::Mesh::assertValidTri_PreAdjacentValid(
      const Triangle& tri,
      const char *msg) const
{
  ACORN_ASSERT_TRI(tri);
  ACORN_ASSERT_TRI_IDX(triIndex(tri));
  ACORN_ASSERT(vert_info_.size() == verts_.size());

  if (number_of_submeshes_ > 0)
  {
    ACORN_ASSERT(number_of_submeshes_ == submesh_tris_.size());
    ACORN_ASSERT(tri.submesh_ >= 0 && tri.submesh_ < number_of_submeshes_);
  }

  for (int dir = 0 ; dir < 3 ; ++dir)
  {
    ACORN_ASSERT_VERT_IDX(tri.verts_[dir]);
    ACORN_ASSERT_EDGE_IDX(tri.edges_[dir].edge_idx_);
    const TriEdge& te = tri.edges_[dir];
    const Edge& edge = edges_[te.edge_idx_];
    ACORN_ASSERT(te.edge_tri_idx_ <= edge.tris_.size());
    const EdgeTri& et = edge.tris_[te.edge_tri_idx_];
    assertValidEdge(edge, msg);

    ACORN_ASSERT(et.tri_idx_ == triIndex(tri));
    ACORN_ASSERT(et.tri_dir_ == dir);

    if (adjacent_tris_valid_ && te.adjacent_tri_ >= 0)
    {
      ACORN_ASSERT_TRI_IDX(te.adjacent_tri_);
      ACORN_ASSERT_DIR(te.adjacent_tri_back_dir_);
      const Triangle& adj = tris_[te.adjacent_tri_];
      int back_dir = te.adjacent_tri_back_dir_;
      const TriEdge& adj_te = adj.edges_[back_dir];

      ACORN_ASSERT(adj_te.edge_idx_ == te.edge_idx_);
      ACORN_ASSERT(adj_te.adjacent_tri_ == triIndex(tri));
      ACORN_ASSERT(adj_te.adjacent_tri_back_dir_ == dir);
    }

    ACORN_ASSERT(edge.verts_[0] < edge.verts_[1]);
    if (edge.verts_[0] == tri.verts_[dir])
    {
      ACORN_ASSERT(edge.verts_[1] == tri.verts_[(dir+1)%3]);
    }
    else
    {
      ACORN_ASSERT(edge.verts_[0] == tri.verts_[(dir+1)%3]);
      ACORN_ASSERT(edge.verts_[1] == tri.verts_[dir]);
    }
  }
}

void mesh_core::Mesh::assertValidEdge(
      const Edge& edge,
      const char *msg) const
{
  int edge_idx = &edge - &edges_[0];
  ACORN_ASSERT_EDGE_IDX(edge_idx);
  for (int i = 0 ; i < 2 ; ++i)
  {
    ACORN_ASSERT_VERT_IDX(edge.verts_[i]);
    int vidx = edge.verts_[i];
    const Vertex& vtx = vert_info_[vidx];

    int ve_cnt = 0;
    for (int j = 0 ; j < vtx.edges_.size() ; ++j)
    {
      ACORN_ASSERT_EDGE_IDX(vtx.edges_[j]);
      if (vtx.edges_[j] == edge_idx)
        ve_cnt++;
    }
    ACORN_ASSERT(ve_cnt == 1);
  }
}


void mesh_core::Mesh::fillGaps()
{
  findSubmeshes();

  for (;;)
  {
    Edge* edge = findGap();
    if (!edge)
      break;
    fillGap(*edge);
  }
}

mesh_core::Mesh::Edge* mesh_core::Mesh::findGap()
{
  std::vector<Edge>::iterator edge = edges_.begin();
  std::vector<Edge>::iterator edge_end = edges_.end();
  for (; edge != edge_end ; ++edge)
  {
    // edge has only 1 tri?
    if (edge->tris_.size() == 1)
      return &*edge;
  }
  return NULL; // no gaps found
}

struct GapEdge
{
  int vert_idx_;
  int edge_idx_;
  int tri_idx_;
  bool correct_winding_;
  bool connected_to_same_tri_;
};

void mesh_core::Mesh::fillGap(Edge& first_edge)
{
  ACORN_ASSERT(first_edge.tris_.size() == 1);

  EdgeTri& first_et = first_edge.tris_[0];
  Triangle &first_tri = tris_[first_et.tri_idx_];

  std::vector<GapEdge> loop;
  loop.reserve(tris_.size());
  int current_vtx = -1;
  int current_edge = edgeIndex(first_edge);

  loop.resize(1);
  loop[0].edge_idx_ = edgeIndex(first_edge);
  loop[0].correct_winding_ = true;
  loop[0].tri_idx_ = first_et.tri_idx_;

  // follow edge in triangle's increasing-dir order 
  if (first_tri.verts_[first_et.tri_dir_] == first_edge.verts_[0])
  {
    loop[0].vert_idx_ = first_edge.verts_[0];
    current_vtx = first_edge.verts_[1];
  }
  else
  {
    ACORN_ASSERT(first_tri.verts_[first_et.tri_dir_] == first_edge.verts_[1]);
    loop[0].vert_idx_ = first_edge.verts_[1];
    current_vtx = first_edge.verts_[0];
  }

  std::vector<GapEdge> new_edges;
  new_edges.reserve(10);

  // look for more gap edges until we have a loop
  int loop_start_index = -1;
  do
  {
    Vertex& vtx = vert_info_[current_vtx];
    new_edges.clear();
    bool found_correct_winding = false;

    // check all edges touching current vertex
    for (int i = 0 ; i < vtx.edges_.size() ; ++i)
    {
      if (vtx.edges_[i] == current_edge)
        continue;

      Edge& vedge = edges_[vtx.edges_[i]];
      if (vedge.tris_.size() == 2)
        continue;

      new_edges.resize(new_edges.size() + 1);
      GapEdge& ge = new_edges.back();

      ge.vert_idx_ = current_vtx;
      ge.edge_idx_ = vtx.edges_[i];
      ge.tri_idx_ = -1; // filled in below
      ge.connected_to_same_tri_ = false;
      ge.correct_winding_ = false;

      // see if there are any triangles along this edge with the correct winding
      for (int j = 0 ; j < vedge.tris_.size() ; ++j)
      {
        EdgeTri& et = vedge.tris_[j];
        Triangle& tri = tris_[et.tri_idx_];
        ge.tri_idx_ = et.tri_idx_;

        // does the edge go in positive winding around triangle?
        if (tri.verts_[et.tri_dir_] == current_vtx)
        {
          if (!found_correct_winding)
          {
            new_edges.clear();
            found_correct_winding = true;
          }
          ge.correct_winding_ = true;
          new_edges.push_back(ge);
        }
        else if (!found_correct_winding)
        {
          new_edges.push_back(ge);
        }
      }
    }

    ACORN_ASSERT(!new_edges.empty());

    // I expect to be able to find a winding in the correct direction for
    // most meshes.
    if (!found_correct_winding)
    {
      logWarn("mesh_core::Mesh::fillGap() found no good edges");
    }

    if (new_edges.size() > 1)
    {
      // this is not necessarily an error.  Just means complex cracks.
      logInform("Found %d gap edges at vtx %d",
        int(new_edges.size()), 
        current_vtx);


      // TODO: use some heuristic to decide which edge to follow?
    }

    ACORN_ASSERT(!new_edges.empty());
    if (new_edges.empty())
    {
      loop_start_index = 0;
      break;
    }

    // Now we have a number of candidate edges.  Use the first one
    loop.push_back(new_edges[0]);

    GapEdge& ge = loop.back();
    Edge& edge = edges_[ge.edge_idx_];

    current_edge = ge.edge_idx_;
    current_vtx = otherVertIndex(edge, current_vtx);

    // check to see if we have a loop.
    for (int i = loop.size() - 2 ; i >= 0 ; --i)
    {
      if (loop[i].vert_idx_ == current_vtx)
      {
        loop_start_index = i;
        break;
      }
    }
  }
  while (loop_start_index == -1);


  // Now we have a loop of vertices around the gap.
  // Generate triangles to fill in the gap

  std::vector<int> loop_verts;
  loop_verts.reserve(loop.size() - loop_start_index);
  for (int i = loop_start_index ;  i < loop.size() ; ++i)
  {
    loop_verts.push_back(loop[i].vert_idx_);
  }

  ACORN_ASSERT(loop_verts.size() >= 3);

  generatePolygon(loop_verts, true);
}

static inline double cross2d(const Eigen::Vector2d& a, const Eigen::Vector2d& b)
{
  return a.x() * b.y() - a.y() * b.x();
}

struct mesh_core::Mesh::GapPoint
{
  enum State
  {
    CONVEX,
    REFLEX,
    EAR,
  };

  State state_;
  int orig_vert_idx_;       // index of vertex in mesh
  Eigen::Vector2d v2d_;     // vertex
  Eigen::Vector2d delta_;   // next.v2d_ - v2d_
  Eigen::Vector2d norm_;    // norm - perpendicular normal pointing inside
  double d_;                // -(norm_ dot v2d_)

  GapPoint *next_;
  GapPoint *prev_;

  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

// Decide if point is an ear or not.
void mesh_core::Mesh::calcEarState(GapPoint& point)
{
  double cr = cross2d(point.prev_->delta_, point.delta_);
  if (cr < 0.0)
  {
    point.state_ = GapPoint::REFLEX;
    return;
  }

  const GapPoint* p[3];
  p[0] = point.prev_;
  p[1] = &point;
  p[2] = point.next_;
  const GapPoint *o = p[2]->next_;

  // assert at least 4 points
  ACORN_ASSERT(p[0] != p[1]);
  ACORN_ASSERT(p[0] != p[2]);
  ACORN_ASSERT(p[0] != o);

  // any other vertices inside the triangle?  If so it is not an ear.
  for (; o != p[0] ; o = o->next_)
  {
    for (int j = 0; j < 3 ; ++j)
    {
      // point is inside triangle.  Not an ear.
      if (j == 3)
      {
        point.state_ = GapPoint::CONVEX;
        return;
      }
        
      double dist = p[j]->norm_.dot(o->v2d_) + p[j]->d_;
      if (dist <= 0.0)
        break;
    }
  }
  point.state_ = GapPoint::EAR;
}

void mesh_core::Mesh::addGapTri(
      const GapPoint *p,
      double direction)
{
  if (direction < 0.0)
  {
    add(p->prev_->orig_vert_idx_,
        p->orig_vert_idx_,
        p->next_->orig_vert_idx_);
  }
  else
  {
    add(p->prev_->orig_vert_idx_,
        p->next_->orig_vert_idx_,
        p->orig_vert_idx_);
  }
}

// generate a polygon given a loop of vertex indices
// if partial_ok is true then it will add at least one tri but may not close
// the entire oplygon.  Otherwise it will close the entire polygon even if the
// polygon has holes.
void mesh_core::Mesh::generatePolygon(
    const std::vector<int> verts,
    bool partial_ok)
{
  int nverts = verts.size();

  if (nverts < 3)
    return;

  if (nverts <= 4)
  {
    add(verts[0], verts[1], verts[2]);
    if (nverts == 4)
      add(verts[1], verts[2], verts[3]);
    return;
  }

  // get array of verts
  EigenSTL::vector_Vector3d points3d;
  points3d.resize(nverts);
  for (int i = 0 ; i < nverts ; i++)
    points3d[i] = verts_[verts[i]];

  // find a plane through (or near) the points
  PlaneProjection proj(points3d);

  // project points onto the plane
  std::vector<GapPoint> points;
  points.resize(nverts);
  int xmin = 0;
  for (int i = 0 ; i < nverts ; i++)
  {
    points[i].orig_vert_idx_ = verts[i];
    points[i].v2d_ = proj.project(points3d[i]);
    if (points[i].v2d_.x() < points[xmin].v2d_.x())
      xmin = i;
  }

  // find winding direction
  // use cross product sign at extreme (xmin) point, guaranteed to be convex.
  // point.cross_ and direction have same sign if corner is convex.
  // direction is positive if loop is ccw
  double direction = 1.0;
  for (int i = 0 ; i < nverts ; i++)
  {
    int idx  = (xmin + i) % nverts;
    int idxn = (xmin + i + 1) % nverts;
    int idxp = (xmin + i + nverts - 1) % nverts;
    GapPoint& p = points[idx];
    GapPoint& pn = points[idxn];
    GapPoint& pp = points[idxp];

    Eigen::Vector2d deltap = p.v2d_ - pp.v2d_;
    Eigen::Vector2d deltan = pn.v2d_ - p.v2d_;
    direction = cross2d(deltap, deltan);

    if (std::abs(direction) > std::numeric_limits<double>::epsilon())
      break;
  }

  // if direction is backwards, use reverse direction
  // After this all processing is done in ccw order
  if (direction < 0.0)
  {
    GapPoint *p = &points[0];
    GapPoint *pp = &points[nverts - 1];
    GapPoint *pn = &points[1];
    for (int i = 0 ; i < nverts ; i++)
    {
      p->next_ = pp;
      p->prev_ = pn;
      pp = p;
      p = pn;
      pn = &points[(i+2)%nverts];
    }
  }
  else
  {
    GapPoint *p = &points[0];
    GapPoint *pp = &points[nverts - 1];
    GapPoint *pn = &points[1];
    for (int i = 0 ; i < nverts ; i++)
    {
      p->next_ = pn;
      p->prev_ = pp;
      pp = p;
      p = pn;
      pn = &points[(i+2)%nverts];
    }
  }

  // find delta_ - vector from point to next point
  // find norm_  - perpendicular to delta_ pointing to inside of polygon
  // find d_     - norm_ dot v2d_
  for (int i = 0 ; i < nverts ; i++)
  {
    GapPoint *p = &points[i];
    p->delta_ = p->next_->v2d_ - p->v2d_;
    p->norm_.x() = -p->delta_.y();
    p->norm_.y() = p->delta_.x();
    p->d_ = p->norm_.dot(p->v2d_);
  }

  // categorize each point
  for (int i = 0 ; i < nverts ; i++)
  {
    calcEarState(points[i]);
  }

  GapPoint *p = &points[0];
  for (;;)
  {
    if (p->state_ != GapPoint::EAR)
    {
      p = p->next_;
      continue;
    }

    // found an ear
    addGapTri(p, direction);

    p->next_->prev_ = p->prev_;
    p->prev_->next_ = p->next_;
    p = p->prev_;

    if (--nverts == 4)
      break;

    p->delta_ = p->next_->v2d_ - p->v2d_;
    p->norm_.x() = -p->delta_.y();
    p->norm_.y() = p->delta_.x();
    p->d_ = p->norm_.dot(p->v2d_);

    calcEarState(*p);
    if (p->state_ != GapPoint::EAR)
    {
      p = p->next_;
      calcEarState(*p);
    }
  }

  // last 4 verts - add last 2 tris
  addGapTri(p, direction);
  addGapTri(p->next_->next_, direction);
}

