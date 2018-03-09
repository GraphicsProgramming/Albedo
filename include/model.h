#pragma once

#include <string>
#include <vector>
using std::string;
using std::vector;

#include <glm/glm.hpp>
using namespace glm;

#include "tiny_obj_loader.h"

#include <embree2\rtcore.h>
#include <embree2\rtcore_ray.h>

using namespace tinyobj;

struct embVert
{
	float x, y, z, a;
};
struct embTriangle
{
	int v0, v1, v2;
};
struct model
{
	unsigned int geom_id;
};
vector<model*> models;

void addObj(RTCScene& scene, string filename, vec3 origin = vec3(), float scale = 1)
{
	printf("Loading .obj file: %s\n", filename.c_str());
	
	vector<shape_t> shapes;
	vector<material_t> materials;

	string err = LoadObj(shapes, materials, filename.c_str(), "models/");

	if (!err.empty())
	{
		printf("\n\nTINYOBJ ERROR: %s\n\n", err.c_str());
	}
	
	printf("Loaded .obj file. Transferring to Embree.\n");
	for (int i = 0; i < shapes.size(); ++i)
	{
		int mesh = rtcNewTriangleMesh(scene, 
			RTC_GEOMETRY_STATIC, 
			shapes[i].mesh.indices.size() / 3,
			shapes[i].mesh.positions.size() / 3);

		// setup vertex buffer
		embVert* verts = (embVert*)rtcMapBuffer(scene, mesh, RTC_VERTEX_BUFFER);
		for (int v = 0; v < shapes[i].mesh.positions.size(); ++v)
			shapes[i].mesh.positions[v] = shapes[i].mesh.positions[v] * scale + origin.x;

		for (int v = 0; v < shapes[i].mesh.positions.size() / 3; ++v)
		{
			verts[v].x = shapes[i].mesh.positions[3 * v + 0];
			verts[v].y = shapes[i].mesh.positions[3 * v + 1];
			verts[v].z = shapes[i].mesh.positions[3 * v + 2];
		}
		rtcUnmapBuffer(scene, mesh, RTC_VERTEX_BUFFER);

		// setup index buffer
		embTriangle* tris = (embTriangle*)rtcMapBuffer(scene, mesh, RTC_INDEX_BUFFER);
		for (int v = 0; v < shapes[i].mesh.indices.size() / 3; ++v)
		{
			tris[v].v0 = shapes[i].mesh.indices[3 * v + 0];
			tris[v].v1 = shapes[i].mesh.indices[3 * v + 1];
			tris[v].v2 = shapes[i].mesh.indices[3 * v + 2];
		}
		rtcUnmapBuffer(scene, mesh, RTC_INDEX_BUFFER);

		// create model
		model* m = new model();
		m->geom_id = mesh;
		models.push_back(m);
		
	}
}
