void construct_light_path(float lambda, vector<light_path_node>& path)
{
	vec3 o;
	vec3 ray;
	
	// Select a triangle
	{
		light_triangle t;
		float r = nrand() * total_light_area;
		for (int i = 0; i < light_triangles.size(); ++i)
		{
			if (r > light_triangles[i].area && i != light_triangles.size() - 1)
			{
				r -= light_triangles[i].area;
				continue;
			}

			t = light_triangles[i];
			break;
		}

		float r1 = nrand();
		float r2 = nrand();
		o = t.p0 + r1 * (t.p1 - t.p0) + r2 * (t.p2 - t.p0);

		light_path_node n;
		n.pos = o;
		n.normal = normalize(cross(t.p1 - t.p0, t.p2 - t.p0));
		//n.ray_towards_camera = rand_hemisphere_ray(n.normal);
		n.mat = models[t.model_id].mat;
		n.prob = 1.f / total_light_area;
		n.accumulated_weight = emmision(lambda, n.mat) * total_light_area;
		path.push_back(n);

		ray = n.ray_towards_camera;
	}

	/*float next_prob = 1.f / (2. * PI);
	while (true)
	{
		light_path_node* last_node = path[path.size() - 1];
		
		// Intersection
		intersection_info info;
		get_intersection_info(o, ray, &info);
		if (info.t < -0.1)
			break;

		vec3 p = info.pos;
		vec3 normal = info.normal;

		light_path_node* n = new light_path_node();
		n->pos = p;
		n->normal = normal;
		n->ray_towards_camera = -ray;
		n->ray_towards_light = vec3(0);
		n->mat = info.mat;
		n->prob = next_prob;
		n->accumulated_weight = last_node->accumulated_weight / next_prob;
		path.push_back(n);

		float emm = emmision(lambda, info.mat);
		if (dot(emm, emm) > 0.001)
			break;
		
		vec3 nextRay = rand_hemisphere_ray(normal);
		float prob = 1 / (2. * PI);
		float weight = BRDF(lambda, p, nextRay, -ray) * max(0.f, dot(normal, -ray));
		
		n->ray_towards_light = nextRay;
		n->accumulated_weight *= weight;

		// Store data for next iteration
		o = p;
		ray = nextRay;
		next_prob = prob;

		// Russian Roulette
		float r = nrand();
		float russian = min(1.f, n->accumulated_weight);
		if (russian < r)
			break;
		
		next_prob *= russian;
	}

	return path;*/
}