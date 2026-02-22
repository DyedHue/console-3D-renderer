#include <iostream>
#include <vector>
#include <cmath>
#include <conio.h>
#include <windows.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
using namespace std;

const float PI = 3.1415926535f;
int row = 100;
int col = 222; //displayed column number is 2 less than this
float hfov = 90;
float cameratoScreen = col / (2 * (tan(((hfov * PI) / 180.0f) / 2)));

vector<char> validInputsMap = { 'w', 'a', 's', 'd', ' ', 'v', 72, 80, 75, 77, 'r', 'o' , 'e', 'q'};
vector<vector<char>> screen(row, vector<char>(col, ' '));
vector<vector<bool>> screenpoints(row, vector<bool>(col, 0));
const char ch = '*';

//Helper functions {
float toradian(float angle) { return ((angle * PI) / 180.0f); }
bool fileExists(const std::string& name) {
	ifstream f(name.c_str());
	return f.good();
}
void setCursorPosition(int x, int y)
{
	static const HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD coord = { (SHORT)x, (SHORT)y };
	SetConsoleCursorPosition(hOut, coord);
}
// }


class point //point on projection plane
{
public:
	int x, y;

	point() : x(0), y(0) {}

	point(int X, int Y)
	{
		x = (row - 1) / 2 - Y, y = X + (col - 1) / 2;
	}
};
class point3d
{
public:
	float x, y, z;

	point3d(){x = 0, y = 0, z = 0;}
	point3d(float X, float Y, float Z){x = X, y = Y, z = Z;}

	bool operator==(const point3d& other) const
	{
		return (x == other.x && y == other.y && z == other.z);
	}
};

class Camera
{
public:
	float x = 0;
	float y = -4;
	float z = 1.5;

	float yaw = 0;  //degrees, towards y positive (North) = 0, right = pos until 360
	float pitch = 90; //degrees, up = 0, down = 180, straight = 90

	void cameramove(char direction, float degree)
	{
		if (direction == 'r')
		{
			yaw += degree;
		}
		else if (direction == 'l')
		{
			yaw -= degree;
		}
		else if (direction == 'u')
		{
			pitch -= degree;
		}
		else if (direction == 'd')
		{
			pitch += degree;
		}
		if (yaw < 0)
		{
			yaw += 360;
		}
		else if (yaw > 360)
		{
			yaw -= 360;
		}
		if (pitch >= 180)
		{
			pitch = 179.9;
		}
		else if (pitch <= 0)
		{
			pitch = 0.1;
		}
	}
	void move(char direction, float amount)
	{
		float modifiedyaw = (90 - yaw); //0 to 360 similar to yaw, changes depending on the direction of movement
		float movementvectorangle, movementvectorx, movementvectory;

		if (!(direction == 'u' || direction == 'd'))
		{
			if (direction == 'b')
			{
				modifiedyaw = (90 - yaw) + 180;
			}
			else if (direction == 'l')
			{
				modifiedyaw = (90 - yaw) + 90;
			}
			else if (direction == 'r')
			{
				modifiedyaw = (90 - yaw) - 90;
			}

			movementvectorangle = modifiedyaw;
			movementvectorx = amount * cos(toradian(movementvectorangle));
			movementvectory = amount * sin(toradian(movementvectorangle));

			x += movementvectorx;
			y += movementvectory;
		}
		else
		{
			if (direction == 'u')
			{
				z += amount;
			}
			else if (direction == 'd')
			{
				z -= amount;
			}
		}
	}
	point3d unitVector()
	{
		float normalyaw = toradian(90 + (360 - yaw));
		float normalpitch = toradian(pitch);

		float sinp = sin(normalpitch);

		float x = sinp * cos(normalyaw);
		float y = sinp * sin(normalyaw);
		float z = cos(normalpitch);

		return { x, y, z };
	}
};
Camera camera;

struct TriangleToRender
{
	point3d p1, p2, p3;
	char symbol;
	float avg_dist;
};
vector<TriangleToRender> queue;

class Model
{
public:
	vector<TriangleToRender> meshTriangles;

	// Helper to apply scale and position offset
	point3d transform(point3d p, float scale, float offX, float offY, float offZ)
	{
		return point3d(p.x * scale + offX, p.y * scale + offY, p.z * scale + offZ);
	}



	void load(string filename, float x, float y, float z, float scale = 1.0f, char symbol = '#')
	{
		meshTriangles.clear();
		vector<point3d> tempVertices;
		ifstream file(filename);

		if (!file.is_open())
		{
			cerr << "Error: Could not open file " << filename << endl;
			return;
		}

		string line;
		while (getline(file, line))
		{
			stringstream ss(line);
			string type;
			ss >> type;

			if (type == "v") // Vertex
			{
				float vx, vy, vz;
				ss >> vx >> vz >> vy;
				//switching y and z for compatibility
				tempVertices.push_back(point3d(vx, vy, vz));
			}
			else if (type == "f") // Face
			{
				vector<int> vertexIndices;
				string segment;

				// Parse indices (e.g., "1/2/3" or "1//3" or "1")
				while (ss >> segment)
				{
					// Find the first slash to isolate the vertex index
					size_t slashPos = segment.find('/');
					string indexStr = (slashPos != string::npos) ? segment.substr(0, slashPos) : segment;

					// OBJ indices are 1-based, convert to 0-based
					vertexIndices.push_back(stoi(indexStr) - 1);
				}

				// Triangulate the face (handles triangles and quads)
				// Creates a triangle fan: (0, 1, 2), (0, 2, 3), etc.
				for (size_t i = 1; i < vertexIndices.size() - 1; ++i)
				{
					TriangleToRender tri;

					// Get raw vertices
					point3d p1Raw = tempVertices[vertexIndices[0]];
					point3d p2Raw = tempVertices[vertexIndices[i]];
					point3d p3Raw = tempVertices[vertexIndices[i + 1]];

					// Apply Scale and Position
					tri.p1 = transform(p1Raw, scale, x, y, z);
					tri.p2 = transform(p2Raw, scale, x, y, z);
					tri.p3 = transform(p3Raw, scale, x, y, z);

					tri.symbol = symbol;
					meshTriangles.push_back(tri);
				}
			}
		}
		file.close();
		cout << "Loaded Model: " << filename << " with " << meshTriangles.size() << " triangles." << endl;
	}

	void addToScene()
	{
		for (const auto& tri : meshTriangles)
		{
			queue.push_back(tri);
		}
	}
};
vector<Model> sceneModels;
void spawnModel(string filename, float x, float y, float z, float scale = 1.0f, char symbol = '#')
{
	Model m;
	m.load(filename, x, y, z, scale, symbol);
	sceneModels.push_back(m);
}

void screenSetConv(int x, int y, char c = ch)
{
	int _row, _col;
	_row = row / 2 - y;
	_col = x + col / 2;
	if (_row < row && _row >= 0 && _col < col && _col >= 0)
		screen[_row][_col] = c;
}
void screenPointSetConv(int x, int y)
{
	int _row, _col;
	_row = row / 2 - y;
	_col = x + col / 2;
	if (_row < row && _row >= 0 && _col < col && _col >= 0)
		screenpoints[_row][_col] = 1;
}

void screenSet(int x, int y, char c = ch)
{
	int _row = y, _col = x;

	if (_row < row && _row >= 0 && _col < col && _col >= 0)
	{
		screen[_row][_col] = c;
	}
}
void screenPointSet(int x, int y, bool clamp = 1)
{
	int _row = y, _col = x;

	if (_row < row && _row >= 0 && _col < col && _col >= 0)
	{
		screenpoints[_row][_col] = 1;
		return;
	}
	if (clamp)
	{
		if (_row < row && _row >= 0)
		{
			if (_col < 0)
			{
				screenpoints[_row][0] = 1;
			}
			else if (_col >= col)
			{
				screenpoints[_row][col - 1] = 1;
			}
		}
	}
}

void pointConnect(const point &point1, const point &point2, bool show = 0)
{
	float x1, y1, x2, y2;

	x1 = point1.y;
	x2 = point2.y;
	y1 = point1.x;
	y2 = point2.x;
	
	if (x1 == x2 && y1 == y2) return 1;

	if (x1 == x2)
	{
		for (int i = min(y1, y2); i <= max(y1, y2); i++)
		{
			screenPointSet(x1, i);
			if (show) screenSet(x1, i);
		}
	}
	else if (y1 == y2)
	{
		for (int i = min(x1, x2); i <= max(x1, x2); i++)
		{
			screenPointSet(i, y1);
			if (show) screenSet(i, y1);
		}
	}
	else
	{
		float m = (y2 - y1) / (x2 - x1);
		float minv = 1 / m;
		float c = y1 - m * x1;

		if (-1 <= m && m <= 1)
		{
			for (int i = min(x1, x2); i <= max(x1, x2); i++)
			{
				int y = round(m * i + c);

				screenPointSet(i, y);
				if (show) screenSet(i, y);
			}
		}
		else
		{
			for (int i = min(y1, y2); i <= max(y1, y2); i++)
			{
				int x = round(minv * (i - c));

				screenPointSet(x, i);
				if (show) screenSet(x, i);
			}
		}
	}
}

void printTriangle(const point &point1, const point &point2, const point &point3, char c = ch)
{
	for (auto& rowVec : screenpoints)
		fill(rowVec.begin(), rowVec.end(), false);

	pointConnect(point1, point2);
	pointConnect(point2, point3);
	pointConnect(point3, point1);


	for (int i = 0; i < row; i++)
	{
		for (int j = 0; j < col; j++)
		{
			if (screenpoints[i][j] == 1)
			{
				bool yes = 0;
				int it;
				for (it = j + 1; it < col; it++)
				{
					if (screenpoints[i][it] == 1)
					{
						yes = 1;
						break;
					}
				}
				if (yes)
				{
					for (int count = j; count <= it; count++)
					{
						screen[i][count] = c;
					}
					j = it - 1;
				}
				else
				{
					screen[i][j] = c;
				}
			}
		}
	}
}

int project3d(const point3d &pointa, char c)
{
	if      (c == 'x') return cameratoScreen * (pointa.x / pointa.y); //col
	else if (c == 'y') return cameratoScreen * (pointa.z / pointa.y); //row
}
point3d transformtoCamSpace(const point3d &w)
{
	float newx = w.x - camera.x;
	float newy = w.y - camera.y;
	float newz = w.z - camera.z;

	float angleRad = toradian(camera.yaw);
	float cosangleRad = cos(angleRad), sinangleRad = sin(angleRad);

	float yawedx = newx * cosangleRad - newy * sinangleRad;
	float yawedy = newx * sinangleRad + newy * cosangleRad;


	angleRad = toradian(-(90 - camera.pitch));
	cosangleRad = cos(angleRad), sinangleRad = sin(angleRad);

	float pitchedy = yawedy * cosangleRad - newz * sinangleRad;
	float pitchedz = yawedy * sinangleRad + newz * cosangleRad;

	return point3d(yawedx, pitchedy, pitchedz);
}
void renderTriangle3d(const TriangleToRender& tri)
{
	float near_plane = 0.1f;
	if (tri.p1.y < near_plane || tri.p2.y < near_plane || tri.p3.y < near_plane)
		return;

	point point1(project3d(tri.p1, 'x'), project3d(tri.p1, 'y'));
	point point2(project3d(tri.p2, 'x'), project3d(tri.p2, 'y'));
	point point3(project3d(tri.p3, 'x'), project3d(tri.p3, 'y'));

	printTriangle(point1, point2, point3, tri.symbol);
}

void addTriangle3d(point3d pointa, point3d pointb, point3d pointc, char c = ch)
{
	TriangleToRender tri = {pointa, pointb, pointc, c};
	queue.push_back(tri);
}
void addFace3d(point3d pointa, point3d pointb, point3d pointc, point3d pointd, char c = ch)
{
	addTriangle3d(pointa, pointb, pointc, c);
	addTriangle3d(pointc, pointd, pointa, c);
}
class block
{
public:
	point3d o;
	block(int x, int y, int z) : o(x, y, z) {}

	void add()
	{
		point3d a(o.x, o.y, o.z);
		point3d b(o.x + 1, o.y, o.z);
		point3d c(o.x + 1, o.y, o.z + 1);
		point3d d(o.x, o.y, o.z + 1);

		point3d e(o.x, o.y + 1, o.z);
		point3d f(o.x + 1, o.y + 1, o.z);
		point3d g(o.x + 1, o.y + 1, o.z + 1);
		point3d h(o.x, o.y + 1, o.z + 1);

		bool backmatched = 0, bottommatched = 0, leftmatched = 0, rightmatched = 0, topmatched = 0, frontmatched = 0;
		/*for (int i = 0; i < queue.size(); i++)
		{
			if (queue[i].p1 == g && queue[i].p2 == h && queue[i].p3 == e)
			{
				queue.erase(queue.begin() + i, queue.begin() + i + 2);
				backmatched = 1;
				i--;
			}
			else if (queue[i].p1 == e && queue[i].p2 ==f && queue[i].p3 == a)
			{
				queue.erase(queue.begin() + i, queue.begin() + i + 2);
				bottommatched = 1;
				i--;
			}
			else if (queue[i].p1 == a && queue[i].p2 == c && queue[i].p3 == e)
			{
				queue.erase(queue.begin() + i, queue.begin() + i + 2);
				leftmatched = 1;
				i--;
			}
			else if (queue[i].p1 == h && queue[i].p2 == f && queue[i].p3 == d)
			{
				queue.erase(queue.begin() + i, queue.begin() + i + 2);
				rightmatched = 1;
				i--;
			}
			else if (queue[i].p1 == g && queue[i].p2 == c && queue[i].p3 == h)
			{
				queue.erase(queue.begin() + i, queue.begin() + i + 2);
				topmatched = 1;
				i--;
			}
			else if (queue[i].p1 == a && queue[i].p2 == b && queue[i].p3 == c)
			{
				queue.erase(queue.begin() + i, queue.begin() + i + 2);
				frontmatched = 1;
				i--;
			}
		}*/
		if (!frontmatched)
		{
			addFace3d(e, f, g, h); //back
		}
		if (!topmatched)
		{
			addFace3d(a, b, f, e, '.');     //bottom
		}
		if (!rightmatched)
		{
			addFace3d(a, d, h, e, '#');    //left
		}
		if (!leftmatched)
		{
			addFace3d(b, c, g, f, '#');    //right
		}
		if (!bottommatched)
		{
			addFace3d(c, d, h, g, '@'); // top
		}
		if (!backmatched)
		{
			addFace3d(a, b, c, d);    //front
		}
	}
};
vector<block> worldBlocks;

void render()
{
	for (auto& blocks : worldBlocks) blocks.add();
	for (auto& model : sceneModels) model.addToScene();

	for (auto& tri : queue)
	{
		tri.p1 = transformtoCamSpace(tri.p1);
		tri.p2 = transformtoCamSpace(tri.p2);
		tri.p3 = transformtoCamSpace(tri.p3);
		tri.avg_dist = (tri.p1.y + tri.p2.y + tri.p3.y) / 3.0f;
	}

	sort(queue.begin(), queue.end(), [](const TriangleToRender& a, const TriangleToRender& b)
										{
											return a.avg_dist > b.avg_dist;
										}
	);

	for (const auto& tri : queue) renderTriangle3d(tri);
}

struct raycastResult
{
	int blockindex = -1;
	point3d placepos;
};
raycastResult raycast(bool tobreak = 0)
{
	point3d unitvect = camera.unitVector();

	point3d currentpos = { camera.x, camera.y, camera.z };
	point3d emptypos = currentpos;

	float advance = 0.05;
	for (float d = 0; d <= 13; d+= advance)
	{
		currentpos.x += unitvect.x * advance;
		currentpos.y += unitvect.y * advance;
		currentpos.z += unitvect.z * advance;

		if (!tobreak && currentpos.z >= -1 && currentpos.z <= 0)
		{
			return { -2, emptypos };
		}
		for(int i = 0; i < worldBlocks.size(); i++)
		{
			if (
				currentpos.x >= worldBlocks[i].o.x && currentpos.x <= worldBlocks[i].o.x + 1 &&
				currentpos.y >= worldBlocks[i].o.y && currentpos.y <= worldBlocks[i].o.y + 1 &&
				currentpos.z >= worldBlocks[i].o.z && currentpos.z <= worldBlocks[i].o.z + 1
			   )
			{
				return { i, emptypos };
			}
		}
		emptypos = currentpos;
	}
	return { -1, {} };
}

void placeBlock()
{
	raycastResult result = raycast();
	if (result.blockindex == -1) return;

	int x = floor(result.placepos.x);
	int y = floor(result.placepos.y);
	int z = floor(result.placepos.z);

	worldBlocks.emplace_back(x, y, z);
}
void breakBlock()
{
	raycastResult result = raycast(1);
	if (result.blockindex == -1) return;

	worldBlocks.erase(worldBlocks.begin() + result.blockindex);
}

static void save()
{
	ofstream outputFile;
	outputFile.open("Save File.txt");
	outputFile << "camera.x=" << camera.x << "\n";
	outputFile << "camera.y=" << camera.y << "\n";
	outputFile << "camera.z=" << camera.z << "\n";
	outputFile << "camera.yaw=" << camera.yaw << "\n";
	outputFile << "camera.pitch=" << camera.pitch << "\n";
	outputFile.close();
}
static void load()
{
	string filename = "Renderer Settings.txt";
	if (!fileExists(filename))
	{
		ofstream outFile(filename);
		outFile << "# row is the number of rows that will be used to show the output in text. Same for col for columns.\nrow=110\ncol=220\n\n# FOV is the Field of View. The higher the FOV, the more you can see on the screen, but the more distorted the image will be.\nfov=90\n\n# If you mess up any settings, you can delete this text file to reset everything to default.";
		outFile.close();
	}

	filename = "Models Positions.txt";
	if (!fileExists(filename))
	{
		ofstream outFile(filename);
		outFile << "Include 3D models here (Only obj files are supported)\n\nHow to use :\nFirst type m. Then after a space, the name of the file (including \".obj\"), after a space, type the x, y and z coordinates (positive z is up) of your desired position to place the model at, each separated by space.\nAfter another space, type the scale of the object (1 means original size), after another space, type a character (8 bit) which is going to be the character that will be used to show the model when it's visible on the screen.\nYou can include 1 model in one line. Any line works.\n\nMore briefly: \"m {filename.obj} {x} {y} {z} {scale} {character}\"\n\nFor example : \"m MyModel.obj 0 3.5 2.89 2 #\" (without the strings) is going to place an object in (x, y, z) = (0, 3.5, 2.89) with 2 times its original size and it's going to show up with the character '#' when running.\n\n";
		outFile.close();
	}


	ifstream savefile;
	savefile.open("Save File.txt");
	string line;
	while (getline(savefile, line))
	{
		if (line.empty()) continue;
		if (line.back() == '\r')
		{
			line.pop_back();
		}
		size_t delimiter_pos = line.find('=');
		string key = line.substr(0, delimiter_pos);
		string value = line.substr(delimiter_pos + 1);
		try
		{
			if (key == "camera.x")
				camera.x = stof(value);
			else if (key == "camera.y")
				camera.y = stof(value);
			else if (key == "camera.z")
				camera.z = stof(value);
			else if (key == "camera.yaw")
				camera.yaw = stof(value);
			else if (key == "camera.pitch")
				camera.pitch = stof(value);
		}
		catch (const std::invalid_argument& e)
		{
			cerr << "Warning: Corrupted or invalid data in save file for key: " << key << endl;
		}
		catch (const std::out_of_range& e)
		{
			cerr << "Warning: Value out of range in save file for key: " << key << endl;
		}
	}
	savefile.close();

	ifstream settings;
	settings.open("Renderer Settings.txt");
	bool settingsChanged = false;
	while (getline(settings, line))
	{
		if (line.empty()) continue;
		if (line.back() == '\r')
		{
			line.pop_back();
		}
		if (line[0] == '#') continue;

		size_t delimiter_pos = line.find('=');
		string key = line.substr(0, delimiter_pos);
		string value = line.substr(delimiter_pos + 1);
		try
		{
			if (key == "fov")
			{
				hfov = stof(value);
				settingsChanged = true;
			}
			else if (key == "row")
			{
				row = stoi(value);
				settingsChanged = true;
			}
			else if (key == "col")
			{
				col = stoi(value) + 2; //adjusting the desired column number to actual column number as 2 colums on the sides are not displayed
				settingsChanged = true;
			}
		}
		catch (const std::invalid_argument& e)
		{
			cerr << "Warning: Corrupted or invalid data in settings file for key: " << key << endl;
		}
		catch (const std::out_of_range& e)
		{
			cerr << "Warning: Value out of range in settings file for key: " << key << endl;
		}
	}
	if (settingsChanged)
	{
		cameratoScreen = col / (2 * (tan(((hfov * PI) / 180.0f) / 2)));
		screen.assign(row, vector<char>(col, ' '));
		screenpoints.assign(row, vector<bool>(col, false));
	}
	settings.close();

	ifstream models;
	models.open("Models Positions.txt");
	while (getline(models, line))
	{
		if (line.empty()) continue;
		if (line.back() == '\r')
		{
			line.pop_back();
		}
		if (!(line[0] == 'm' && line[1] == ' '))
		{
			continue;
		}

		size_t delimiter_pos;

		line = line.substr(2);
		delimiter_pos = line.find(' ');
		string name = line.substr(0, delimiter_pos);

		line = line.substr(delimiter_pos + 1);
		delimiter_pos = line.find(' ');
		float x = stof(line.substr(0, delimiter_pos));

		line = line.substr(delimiter_pos + 1);
		delimiter_pos = line.find(' ');
		float y = stof(line.substr(0, delimiter_pos));

		line = line.substr(delimiter_pos + 1);
		delimiter_pos = line.find(' ');
		float z = stof(line.substr(0, delimiter_pos));

		line = line.substr(delimiter_pos + 1);
		delimiter_pos = line.find(' ');
		float scale = stof(line.substr(0, delimiter_pos));

		line = line.substr(delimiter_pos + 1);
		char symbol = line[0];

		try
		{
			spawnModel(name, x, y, z, scale, symbol);
		}
		catch (const std::invalid_argument& e)
		{
			cerr << "Warning: Corrupted or invalid data in the Models Positions file" << endl;
		}
	}
	models.close();
}

void show()
{
	screen[row / 2][col / 2] = 'O';
	setCursorPosition(0, 0);

	string fullFrame = "";
	fullFrame.reserve(row * (col * 2) + 2*col + 2*row + 10);
	for (int i = 0; i < col * 2 - 2; i++)
		fullFrame += "_";
	fullFrame += "\n";

	for (int i = 0; i < row; i++)
	{
		fullFrame += "|";
		for (int j = 1; j < col - 1; j++)
		{
			fullFrame += screen[i][j];
			fullFrame += ' ';
		}
		fullFrame += "|\n";
	}
	for (int i = 0; i < col * 2 - 2; i++)
		fullFrame += "_";

	printf("%s", fullFrame.c_str());
	cout << "\nPosition: " << camera.x << " " << camera.y << " " << camera.z << " Yaw: " << camera.yaw << " Pitch: " << camera.pitch
		<< "                                        \n";


	for (auto& rowVec : screen)
		fill(rowVec.begin(), rowVec.end(), ' ');
}

void road()
{
	/*point3d pointa; pointa.set(10, 50, -5);
	point3d pointb; pointb.set(-10, 50, -5);*/

	point3d pointc(10, -9, -5);
	point3d pointd(-10, -9, -5);
	point3d pointe(10, -3, -5);
	point3d pointf(-10, -3, -5);

	addTriangle3d(pointc, pointd, pointe, '@');
	addTriangle3d(pointd, pointe, pointf, '@');
}
void placeHouse(float x, float y, float z)
{
	for (int i = 0; i <= 4; i++)
	{
		for (int j = 3; j >= 0; j--)
		{
			worldBlocks.emplace_back(x + i, y + -1 * 4, z + j);
			if (i == 2 && (j == 1 || j == 0))
				continue;
			worldBlocks.emplace_back(x + i, y, z + j);
		}
	}
	for (int i = -1; i >= -3; i--)
	{
		for (int j = 3; j >= 0; j--)
		{
			worldBlocks.emplace_back(x, y + i, z + j);
			worldBlocks.emplace_back(x + 4, y + i, z + j);
		}
	}

	for (int i = -5; i <= 1; i++)
		worldBlocks.emplace_back(x - 1, y + i, z + 4),
		worldBlocks.emplace_back(x + 5, y + i, z + 4);
	for (int i = 0; i <= 4; i++)
		worldBlocks.emplace_back(x + i, y - 5, z + 4),
		worldBlocks.emplace_back(x + i, y + 1, z + 4);
	for(int i = -4; i <= 0; i++)
		worldBlocks.emplace_back(x, y + i, z + 5),
		worldBlocks.emplace_back(x+4, y + i, z + 5);
	for(int i = 1; i<= 3; i++)
		worldBlocks.emplace_back(x + i, y - 4, z + 5),
		worldBlocks.emplace_back(x + i, y, z + 5),
		worldBlocks.emplace_back(x + i, y - 3, z + 6),
		worldBlocks.emplace_back(x + i, y - 1, z + 6);

	worldBlocks.emplace_back(x + 3, y - 2, z + 6);
	worldBlocks.emplace_back(x + 1, y - 2, z + 6);

	worldBlocks.emplace_back(x + 2, y - 2, z + 7);
}
void tree(float x, float y, float z)
{
	for (int i = 0; i < 4; i++)
		worldBlocks.emplace_back(x, y, z+i);

	for(int i = -2; i <= 2; i++)
		for(int j = -2; j <= 2; j++)
			worldBlocks.emplace_back(x+i, y+j, z+4);

	for (int i = -1; i <= 1; i++)
		for (int j = -1; j <= 1; j++)
			worldBlocks.emplace_back(x + i, y + j, z+5);

	worldBlocks.emplace_back(x, y, z + 6);
}
void showHello(float x = 0, float y = 0, float z = 0)
{
	for (int i = 0; i <= 4; i++)
	{
		worldBlocks.emplace_back(x, y, z + i);
		worldBlocks.emplace_back(x + 3, y, z + i);

		worldBlocks.emplace_back(x + 5, y, z + i);

		worldBlocks.emplace_back(x + 10, y, z + i);

		worldBlocks.emplace_back(x + 15, y, z + i);
	}
	worldBlocks.emplace_back(x + 1, y, z + 2);
	worldBlocks.emplace_back(x + 2, y, z + 2);
	for (int i = 1; i <= 3; i++)
	{
		worldBlocks.emplace_back(x + i + 5, y, z);
		worldBlocks.emplace_back(x + i + 5, y, z + 2);
		worldBlocks.emplace_back(x + i + 5, y, z + 4);

		worldBlocks.emplace_back(x + 20, y, z + i);

		worldBlocks.emplace_back(x + 23, y, z + i);
	}
	for (int i = 6; i <= 13; i++)
	{
		worldBlocks.emplace_back(x + i + 5, y, z);
		if (i == 8) i = 10;
	}
	worldBlocks.emplace_back(x + 21, y, z);
	worldBlocks.emplace_back(x + 22, y, z);
	worldBlocks.emplace_back(x + 21, y, z + 4);
	worldBlocks.emplace_back(x + 22, y, z + 4);
}

void prepareWorld()
{
	
	//spawnModel("HalfSphere.obj", 0, 0, 1.5, 1.0f, '#');
	//spawnModel("Castle OBJ.obj", 0, 10, 0, 1.0f, '#');
	//spawnModel("MapleTree.obj", 20, 20, 0, 1.0f, '#');
	/*spawnModel("MapleTreeLeaves.obj", 20, 20, 0, 1.0f, '(');
	spawnModel("MapleTreeStem.obj", 20, 20, 0, 1.0f, '#');*/

	/*addTriangle3d({ 0, 0, 0 }, { 1, 0, 1 }, { 1, 0, 0 }, '@');*/
	
	/*showHello(-40, 12, 0);
	placeHouse(-5, -5, 0);
	placeHouse(-18, -5, 0);

	tree(-24, -7, 0);*/
}

void action(char c)
{
	if (c == 'w') camera.move('f', 0.5);
	else if (c == 's') camera.move('b', 0.5);
	else if (c == 'a') camera.move('l', 0.5);
	else if (c == 'd') camera.move('r', 0.5);

	else if (c == ' ') camera.move('u', 0.5);
	else if (c == 'v') camera.move('d', 0.5);

	else if (c == 72) camera.cameramove('u', 5);
	else if (c == 75) camera.cameramove('l', 5);
	else if (c == 80) camera.cameramove('d', 5);
	else if (c == 77) camera.cameramove('r', 5);

	else if (c == 'r') system("cls");

	else if (c == 'o') save();

	else if (c == 'e') placeBlock();
	else if (c == 'q') breakBlock();
}

int main()
{
	ios_base::sync_with_stdio(false);
	cin.tie(NULL);
	
	load();
	prepareWorld();
	int inp;
	while (1)
	{
		queue.clear();
		render();
		show();
		while (1)
		{
			inp = _getch();
			if (find(validInputsMap.begin(), validInputsMap.end(), inp) != validInputsMap.end()) break;
		}
		action(inp);
	}
	return 0;
}