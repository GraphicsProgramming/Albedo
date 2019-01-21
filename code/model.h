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

struct material
{
	float light_intensity = 0;
	float diffuse_albedo = 1.f;
	float diffuse_mean = 540;
	float diffuse_stddev = 40;
};
struct model
{
	material mat;
	unsigned int geom_id;
};
vector<model> models;

struct light_triangle
{
	vec3 p0, p1, p2;
	float area;
	int model_id;
};
vector<light_triangle> light_triangles;
float total_light_area = 0.f;

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
		model cur_model;
		cur_model.geom_id = mesh;

		int material_id = shapes[i].mesh.material_ids[0];
		float e = materials[material_id].emission[0];
		cur_model.mat.light_intensity = 3.f * e / 440.f;
		if (e > 0.1)
		{
			// Add the triangles to the light triangle cache
			for (int v = 0; v < shapes[i].mesh.indices.size() / 3; ++v)
			{
				int v0 = shapes[i].mesh.indices[3 * v + 0];
				int v1 = shapes[i].mesh.indices[3 * v + 1];
				int v2 = shapes[i].mesh.indices[3 * v + 2];

				light_triangle t;
				t.p0 = vec3(verts[v0].x, verts[v0].y, verts[v0].z);
				t.p1 = vec3(verts[v1].x, verts[v1].y, verts[v1].z);
				t.p2 = vec3(verts[v2].x, verts[v2].y, verts[v2].z);
				t.area = length(cross(t.p1 - t.p0, t.p2 - t.p0)) / 2.f;
				t.model_id = models.size() - 1;
				light_triangles.push_back(t);

				total_light_area += t.area;
			}
		}

		// converting between rgb colors and wavelength responses go here
		// for now, just hardcoding some colors
		if (i == 3)
		{
			cur_model.mat.diffuse_albedo = 0.8;
			cur_model.mat.diffuse_mean = 680;
			cur_model.mat.diffuse_stddev = 40;
		}
		else if (i == 5)
		{
			cur_model.mat.diffuse_albedo = 0.8;
			cur_model.mat.diffuse_mean = 540;
			cur_model.mat.diffuse_stddev = 40;
		}
		else
		{
			cur_model.mat.diffuse_albedo = 0.5;
			cur_model.mat.diffuse_mean = 600;
			cur_model.mat.diffuse_stddev = 200;
		}
		
		models.push_back(cur_model);
	}
}
