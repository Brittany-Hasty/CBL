#include <assert.h>
#include <string>
#include <functional>
#include <sstream>
#include <experimental/filesystem>

#include "axis.h"

//For every helix in a pdb file, detects if the helix folds in half, or drastically changes direction
	//it then outputs the point where the helix switches direction

using namespace cbl;

std::vector<pdb> runAxisComparisonForHelixGeneration(std::string pdb_path)
{
	using path = std::experimental::filesystem::path;

	// Convert relative to absolute path if necessary, using experimental std lib
	// If there are symbolic links, we also convert those to canonical form
	path symbolic_pdb_path = pdb_path;
	symbolic_pdb_path = std::experimental::filesystem::canonical(symbolic_pdb_path);
	pdb_path = symbolic_pdb_path.string();

	// Assert that the extension is ".pdb"
	assert(symbolic_pdb_path.extension() == ".pdb");

	// Make output directory for results to be populated
	path output_path = symbolic_pdb_path.parent_path().string() + "/output";
	std::experimental::filesystem::create_directory(output_path);

	// Get path stripped of file extension for input into axis-comparsion
	path stripped_path = symbolic_pdb_path.parent_path().string() + "/" +
		symbolic_pdb_path.stem().string();

	std::string command = "leastsquare.exe \"" + stripped_path.string()
		+ "\" \"Empty\" \"Empty\" \"Empty\"";

	// Execute axis comparison
	system(command.c_str());

	// Read all files in this directory that begin with "trueHelix"

	std::vector<cbl::pdb> matched_helix;

	for (auto &p : std::experimental::filesystem::directory_iterator(output_path))
	{
		auto s = p.path().filename().string();

		// If begins with "trueHelix"
		if (s.find("trueHelix") == 0)
		{
			matched_helix.emplace_back(p.path().string());
		}
	}

	// Rename output directory to something we can refer to later

	path new_output_path = symbolic_pdb_path.parent_path().string() + "/"
		+ symbolic_pdb_path.stem().string();

	std::experimental::filesystem::remove_all(new_output_path);

	std::experimental::filesystem::rename(output_path, new_output_path);

	return matched_helix;
}

int main(int argc, char* argv[])
{
	int num_args = 1;
	assert(argc == num_args + 1 && "Incorrect num arguments, 1 input: pdb");

	std::string pdb_file_path_in = argv[1];
	std::vector<pdb> helices = runAxisComparisonForHelixGeneration(pdb_file_path_in);
	size_t period_pos = pdb_file_path_in.find_last_of('.');
	pdb_file_path_in.resize(period_pos);

	int helixFileNum = 1;

	for (size_t i = 0; i < helices.size(); i++)
	{
		pdb& helix = helices[i];
		pdb &structure = helix;

		//test #2: detection using net distance

		std::vector<int> vertices;
		std::vector<cbl::real> distances;

		cbl::point p1 = structure[0];
		cbl::point p2 = structure[structure.size()-1];

		//find discrepencies in distance from generated line
		for (size_t i = 0; i < structure.size(); i++)
		{
			cbl::point p3 = structure[i];

			//makes a virtual line from p1 to p2 and finds the distance to the line from p3
			cbl::real distance = distToLine(p1,p2,p3);

			distances.push_back(distance);
		}

		std::cout << std::endl;

		for (size_t i = 1; i < distances.size()-1; i++)
		{
			//try to add index location to a vector if the discrepency is notable

			if (distances[i] > 5)
			{
				//checks if a point is a local maxima

				if ((distances[i] > distances[i - 1]) && (distances[i] > distances[i + 1]))
				{
					std::cout << "Detected vertex at index: " << i << std::endl;

					vertices.push_back(i);
				}

				//if two points tie as a local maxima, the first point is chosen and the second is skipped

				if ((distances[i] > distances[i - 1]) && (distances[i] == distances[i + 1]))
				{
					if ((size_t)(i + 2) < distances.size())
					{
						if (distances[i + 1] > distances[i + 2])
						{
							std::cout << "Detected vertex at index: " << i << std::endl;
							vertices.push_back(i);
							i++;
						}
					}
				}
			}
		}

		//list points inbetween the two segments if vertices are detected

		if (vertices.size() > 0)
		{
			pdb halfPoints;
			int l_shift, r_shift;
			cbl::real x1, y1, z1;
			size_t l_bound, r_bound;


			if (vertices.size() > 1)
			{
				for (size_t i = 0; i < vertices.size() - 1; i++)
				{
					l_shift = vertices[i];
					r_shift = vertices[i];

					//long if/else statement to find the intervals between two maxima

					if (i == 0)
					{
						//The first interval for a helix with multiple vertices

						l_bound = 1;
						r_bound = vertices[i + 1]-1;
					}
					else if ((i != 0) && (i != vertices.size() - 1))
					{
						//The middle interval(s) for a helix with multiple vertices

						l_bound = vertices[i - 1]+1;
						r_bound = vertices[i + 1]-1;
					}
					else
					{
						//The final interval for a helix with multiple vertices

						l_bound = vertices[i - 1]+1;
						r_bound = structure.size()-2;
					}
				}
			}
			else
			{
				//The interval for a helix with one vertex (i.e. the entire interval for the helix)

				l_shift = vertices[0];
				r_shift = vertices[0];

				l_bound = 1;
				r_bound = structure.size()-2;
			}

			while(l_shift >= (int)l_bound && r_shift < (int)r_bound)
			{
				//calculates points inbetween the two sides

				l_shift -= 1;
				r_shift += 1;

				x1 = (structure[l_shift].x + structure[r_shift].x) / 2;
				y1 = (structure[l_shift].y + structure[r_shift].y) / 2;
				z1 = (structure[l_shift].z + structure[r_shift].z) / 2;

				halfPoints.emplace_back(x1, y1, z1);
			}
			
				
			//double check just in case halfpoints weren't generated

			if (halfPoints.size() > 0)
			{
				//output results

				std::string halfPointsFileName = pdb_file_path_in;
				halfPointsFileName = halfPointsFileName + "/" + halfPointsFileName + "_helix"
					+ std::to_string(helixFileNum) + "_halfPoints.pdb";

				halfPoints.write(halfPointsFileName);

				std::cout << "\nHalf Points written to file: " << halfPointsFileName << std::endl << std::endl;
			}
			else
			{
				std::cout << "\nHalf points generation error!" << std::endl;
			}
		}

		//No notable bends in the helix were detected

		else
		{
			std::cout << "\nNo fold detected!\n" << std::endl;
		}

		helixFileNum++;

	}

	std::cout << std::endl;

	system("PAUSE");

	return 0;
}