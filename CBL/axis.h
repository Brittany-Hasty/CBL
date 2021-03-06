#pragma once

#include <sstream>

#include "core.h"
#include "pdb.h"
#include "mrc.h"

// the axis object represents a single interpolated helix or strand structure.

namespace cbl
{
	class axis
	{
	public:
		axis(pdb &p)
		{
			for (size_t i = 0; i < p.size(); i++)
			{
				points.push_back((point)p[i]);
			}

			error.resize(p.size());
		}

		void write(std::string file_name, real threshold)
		{
			std::ofstream file(file_name);
			assert(file && "Could not open axis file for writing");
			assert(points.size() && "Attempted to write an empty axis file");

			for (size_t i = 0; i < points.size(); i++)
			{
				real t = (real)i / (real)(points.size() - 1);
				file << t << " " << threshold << " " << error[i] << std::endl;
			}
		}

		void write(std::ostringstream& out, real threshold)
		{
			for (size_t i = 0; i < points.size(); i++)
			{
				real t = (real)i / (real)(points.size() - 1);
				out << t << " " << threshold << " " << error[i] << std::endl;
			}
		}

		void mergeError(real merge_length)
		{
			// Loop across points, merging points and associated error when length is surpassed

			std::vector<point> new_points;
			std::vector<real> new_errors;

			std::vector<point> temp_stack;
			std::vector<size_t> temp_stack_index;

			real length = 0;

			if (points.size())
			{
				temp_stack.push_back(points[0]);
				temp_stack_index.push_back(0);
			}
				

			for (size_t i = 1; i < points.size(); i++)
			{
				auto p_prev = points[i - 1];
				auto p = points[i];

				temp_stack.push_back(p);
				temp_stack_index.push_back(i);

				length += p.dist(p_prev);

				if (length > merge_length)
				{
					// Get average pos of points so far on temp stack

					length = 0;

					real x = 0, y = 0, z = 0;
					
					for (size_t j = 0; j < temp_stack.size(); j++)
					{
						x += temp_stack[j].x;
						y += temp_stack[j].y;
						z += temp_stack[j].z;
					}

					x /= temp_stack.size();
					y /= temp_stack.size();
					z /= temp_stack.size();

					new_points.emplace_back(x, y, z);

					// Get average error of points

					real sum_error = 0;

					for (size_t j = 0; j < temp_stack_index.size(); j++)
					{
						sum_error += error[temp_stack_index[j]];
					}

					sum_error /= temp_stack_index.size();

					new_errors.push_back(sum_error);

					temp_stack.clear();
					temp_stack_index.clear();
				}
			}

			points = new_points;
			error = new_errors;
		}

		void calcError(mrc &m, std::function<real(std::vector<real>)> fold)
		{
			// For each voxel, find the closest pdb point, then register that error with that point

			std::vector<std::vector<real>> error_registrations;
			error_registrations.resize(points.size());

			for (size_t i = 0; i < m.map.size(); i++)
			{
				point voxel = m.getTransformedVoxel(i);
				real density = m.map[i];

				if (density > 0)
				{
					real min_dist = Infinity;
					size_t min_point = 0;

					for (size_t j = 0; j < points.size(); j++)
					{
						point p = points[j];

						// std::cout << p.x << std::endl;

						real square_dist = distSq(voxel, p);

						// std::cout << voxel.x << " " << p.x << std::endl;

						// std::cout << square_dist << std::endl;

						if (square_dist < min_dist)
						{
							min_dist = square_dist;
							min_point = j;
						}
					}

					min_dist = sqrt(min_dist);

					error_registrations[min_point].push_back(min_dist);
				}
			}

			// Apply fold on error registrations to get actual error per point

			for (size_t i = 0; i < error_registrations.size(); i++)
			{
				if (error_registrations[i].size() > 0)
				{
					error[i] = fold(error_registrations[i]);
				}
				else
				{
					error[i] = 0;
				}
			}
		}

		void removeZeroErrors()
		{
			for (size_t i = 0; i < error.size(); )
			{
				if (error[i] == 0)
				{
					points.erase(points.begin() + i);
					error.erase(error.begin() + i);
				}
				else
				{
					i++;
				}
			}
		}
	public:

		std::vector<point> points;
		std::vector<real> error;
	};
}
