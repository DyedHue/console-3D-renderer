#define NCURSES_MOUSE_VERSION 2
#define PDC_NCMOUSE

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <array>

#include <memory>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

#include <chrono>
#include <windows.h>
#include <conio.h>

#define HAS_NCURSES 0
#if __has_include(<curses.h>)
	#include <curses.h>
	#define HAS_NCURSES 1
#endif

using namespace std;



float debugbrightness;



const float PI = 3.141593f;

int row = 100;
int col = 222; //displayed column number is 2 less than this
float hfov = 90;
float camSpeedMouse = 10, camSpeedKeyboard = 60, walkSpeed = 2.5;
int renderDistance = 13;
bool useNcurses = true;

float equationRayStep = 0.05;
float equationSolveTolerance = 0.1;

vector<vector<char>> screen;
vector<vector<bool>> screenpoints;
float focalLengthpx;
//const string brightnessSymbols = ".,-~:;=!*#$@";
const string brightnessSymbols = ".,:-;!~*=#$@";
//const string brightnessSymbols = " .-~:+=xX$#@WMB8&%Q0OZX";
//const string brightnessSymbols = "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'. ";
const char ch = '*';

map<int, bool>isPressed = { {'W', false}, {'S', false}, {'A', false}, {'D', false}, {VK_CONTROL, false},
							{' ', false}, {VK_SHIFT, false},
							{VK_UP, false}, {VK_DOWN, false}, {VK_LEFT, false}, {VK_RIGHT, false},
							{'E', false}, {'Q', false},
							{'R', false}, {'O', false}, 
	                        {VK_ESCAPE, false},
	                        {VK_LBUTTON, false}, {VK_RBUTTON, false}};
map<int, bool> wasPressed = isPressed;

POINT screenCenter;
float deltaTime = 0;
bool mouseLocked = 1;

const string defaultSettingsText = "# FOV is the Field of View. The higher the FOV, the more you can see on the screen, but the more distorted the image will be.\nfov=90\n\n#blocks/s and degrees/s\nwalk_speed=2.5\ncam_speed_mouse=10\ncam_speed_keyboard=80\n\n# Makes screen refresh and mouse inputs better. But if it doesn't work properly, disable it.\nuse_ncurses=1\n\n# row is the number of rows that will be used to show the output in text. Same for col for columns.\n# This will be ignored when running with Ncurses enabled as it will dynamically adjust that.\nrow=110\ncol=220\n\n# If you mess up any settings, you can delete this text file to reset everything to default.";
const string defaultModelspositionsText = "# You can import 3D models here as `.obj` files only. Keep your obj file in the same directory as the exe.\n# 1. Go to `Models Positions.txt` within the same directory as the exe.\n# 2. First type the name of the file (including `.obj`).\n# 3. After a space, type the x, y and z coordinates (positive z is up) of your desired position to place the model at, each separated by space.\n# 4. After another space, type the **scale** of the object (1 means original size).\n\n# More briefly: `{filename.obj} {x} {y} {z} {scale}`\n\n# For example : `MyModel.obj 0 3.5 2.89 2` is going to place the `MyModel.obj` model in (x, y, z) = (0, 3.5, 2.89) with 2 times its base size.\n\n# You can include 1 model in one line. Any line works. Any number of model works.\n# You can make a comment line by starting with a `#`.\n\n";
auto startTime = chrono::high_resolution_clock::now();
auto currentTime = startTime;

//Helper functions {
float toradian(float angle) { return ((angle * PI) / 180.0f); }
float todegree(float angle) { return (angle * (180.0f/PI)); }
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
void constructScreen()
{
	screen.assign(row, vector<char>(col, ' '));
	screenpoints.assign(row, vector<bool>(col, false));
	focalLengthpx = col / (2 * (tan(((hfov * PI) / 180.0f) / 2)));
}
bool justReleased(int button)
{
	return !isPressed[button] && wasPressed[button];
}
bool justPressed(int button)
{
	return isPressed[button] && !wasPressed[button];
}
float ftoi_power(float n, int ex) //only upto 3
{
	switch (ex)
	{
	    case 0: return 1;
	    case 1: return n;
		case 2: return n * n;
		case 3: return n * n * n;
		case 4: return n * n * n * n;
		case 5: return n * n * n * n * n;
	}
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

	point3d(float X, float Y, float Z) : x(X), y(Y), z(Z) {}

	bool operator==(const point3d& other) const
	{
		return (x == other.x && y == other.y && z == other.z);
	}

	float operator*(const point3d& other) const //dot product (considering vectors)
	{
		return x * other.x + y * other.y + z * other.z;
	}
	point3d operator+(const point3d& other) const //adding two vectors
	{
		return point3d(x + other.x, y + other.y, z + other.z);
	}
	point3d operator-(const point3d& other) const //subtracting two vectors
	{
		return point3d(x - other.x, y - other.y, z - other.z);
	}

	point3d operator*(const float& mag) const //multiplying a vector by a scalar (considering vectors)
	{
		return point3d(x * mag, y * mag, z * mag);
	}
	point3d normalized() const//use if you treat a point3d as a vector3
	{
		float l = sqrt(x * x + y * y + z * z);
		if (l != 0)
		{
			return point3d(x / l, y / l, z / l);
		}
		return point3d(0, 0, 0);
	}
	float magnitude() const //use if you treat a point3d as a vector3
	{
		return sqrt(x * x + y * y + z * z);
	}
};

point3d lightdir = point3d(0, 0, -1);

class Camera
{
public:
	float x = 0;
	float y = -4;
	float z = 1.5;

	float yaw = 0;  //degrees, towards y positive (North) = 0, right = positive until 360
	float pitch = 90; //degrees, up = 0, down = 180, straight = 90

	void directionMove()
	{
		POINT mousePos;
		GetCursorPos(&mousePos);
		if (mouseLocked)
		{
			int mouse_dx = mousePos.x - screenCenter.x;
			int mouse_dy = mousePos.y - screenCenter.y;

			yaw += mouse_dx * camSpeedMouse * deltaTime;
			pitch += mouse_dy * camSpeedMouse * deltaTime;

			SetCursorPos(screenCenter.x, screenCenter.y);
		}

		if (isPressed[VK_RIGHT])
			yaw += camSpeedKeyboard * deltaTime;
		if (isPressed[VK_LEFT])
			yaw -= camSpeedKeyboard * deltaTime;
		if (isPressed[VK_UP])
			pitch -= camSpeedKeyboard * deltaTime;
		if (isPressed[VK_DOWN])
			pitch += camSpeedKeyboard * deltaTime;

		if (yaw < 0)
			yaw += 360;
		else if (yaw > 360)
			yaw -= 360;
		if (pitch >= 180)
			pitch = 179.9;
		else if (pitch <= 0)
			pitch = 0.1;
	}
	void positionMove()
	{
		point3d moveVec = {0, 0, 0};

		if (isPressed['W'])
			moveVec.y++;
		if (isPressed['S'])
			moveVec.y--;
		if(isPressed['D'])
			moveVec.x++;
		if(isPressed['A'])
			moveVec.x--;
		if(isPressed[' '])
			moveVec.z++;
		if(isPressed[VK_CONTROL])
			moveVec.z--;

		float sinyaw = sin(toradian(yaw)), cosyaw = cos(toradian(yaw));
		float newx = moveVec.x * cosyaw + moveVec.y * sinyaw;
		float newy = -moveVec.x * sinyaw + moveVec.y * cosyaw;

		moveVec = {newx, newy, moveVec.z};
		moveVec = moveVec.normalized();

		float moveAmount = walkSpeed * deltaTime * (1 + 2 * isPressed[VK_SHIFT]);
		x += moveVec.x * moveAmount;
		y += moveVec.y * moveAmount;
		z += moveVec.z * moveAmount;
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

class TriangleToRender
{
public:
	point3d p1, p2, p3;
	char symbol;
	point3d avgNor = { 0, 0, 1 };
	float avg_dist;
	point3d n1 = { 0, 0, 1 }, n2 = { 0, 0, 1 }, n3 = { 0, 0, 1 };

	void setAvgNormal()
	{
		avgNor = (n1 + n2 + n3) * (1.0f / 3.0f);
	}
};
vector<TriangleToRender> queue;

class equation
{
public:
	point3d pos{0, 0, 0};
	point3d rot{0, 0, 0};
	point3d scaleAxis{1, 1, 1};
	float scale = 1;

	virtual ~equation() = default;

	float evaluate(const point3d& p) const
	{
		point3d newp = {(p.x - pos.x)/(scale * scaleAxis.x), (p.y - pos.y)/(scale * scaleAxis.y), (p.z - pos.z)/(scale * scaleAxis.z)};
		//implement rotation here
		return evaluateLocal(newp.x, newp.y, newp.z);
	}
	point3d gradient(const point3d& p) const
	{
		point3d newp = {(p.x - pos.x)/(scale * scaleAxis.x), (p.y - pos.y)/(scale * scaleAxis.y), (p.z - pos.z)/(scale * scaleAxis.z)};
		return gradientLocal(newp.x, newp.y, newp.z).normalized();
	}
protected:
	virtual float evaluateLocal(const float x, const float y, const float z) const = 0;
	virtual point3d gradientLocal(const float x, const float y, const float z) const = 0;
};
class sphere : public equation
{
public:
	float r = 1;
protected:
	float evaluateLocal(const float x, const float y, const float z) const override
	{
		return x*x + y*y + z*z - r*r;
	}
	point3d gradientLocal(const float x, const float y, const float z) const override
	{
		return {x, y, z};
	}
};
class torus : public equation
{
public:
	float R = 0.8, r = 0.5;
protected:
	float evaluateLocal(const float x, const float y, const float z) const override
	{
		return ftoi_power(sqrt(x*x + y*y) - R, 2) + z*z - r*r;
	}
	point3d gradientLocal(const float x, const float y, const float z) const override
	{
		float dist = sqrt(x*x + y*y);
		return {2 * x * (1- R/dist), 2 * y * (1- R/dist), 2*z};
	}
};
class heart : public equation
{
protected:
	float evaluateLocal(const float x, const float y, const float z) const override
	{
		float xx = x*x, yy = y*y, zzz = z*z*z;
		return ftoi_power(xx + (9.0/4.0) * yy + z*z - 1, 3) - xx * zzz - (9.0/200.0) * yy * zzz;
	}
	point3d gradientLocal(const float x, const float y, const float z) const override
	{
		float zz = z*z, xx = x*x, yy = y*y;
		float zzz = z*z*z;

		float common = 3 * ftoi_power(xx + 2.25f * yy + zz - 1, 2);

		return {2 * x * (common - zzz), y * (common * 4.5f - 0.09f * zzz), common * 2 * z - 3 * zz * (xx + 0.045f * yy)};
	}
};
class ripples : public equation
{
protected:
	float evaluateLocal(const float x, const float y, const float z) const override
	{
		return sin(x*x + y*y) - z;
	}
	point3d gradientLocal(const float x, const float y, const float z) const override
	{
		return {x, y, z};
	}
};
class line2D: public equation
{
public:
	float m = 2, c = 3;
protected:
	float evaluateLocal(const float x, const float y, const float z) const override
	{
		float ans = m * x + c - y;
		if(z > -0.1 && z < 0.1)
			return ans;
		else return ans <= 0? -FLT_MAX: FLT_MAX;
	}
};

vector<unique_ptr<equation>>Equations;
heart* heartRef;


class Model
{
public:
	vector<TriangleToRender> meshTriangles;

	point3d transform(point3d p, float scale, float offX, float offY, float offZ)
	{
		return point3d(p.x * scale + offX, p.y * scale + offY, p.z * scale + offZ);
	}

	void load(string filename, float x, float y, float z, float scale = 1.0f, char symbol = '#')
	{
		meshTriangles.clear();
		
		vector<point3d> tempVertices;
		vector<point3d> Normals;

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

			if (type == "v")
			{
				float vx, vy, vz;
				ss >> vx >> vz >> vy;
				tempVertices.push_back(point3d(vx, vy, vz)); //switching y and z for compatibility
			}
			if (type == "vn")
			{
				float nx, ny, nz;
				ss >> nx >> nz >> ny;
				Normals.push_back(point3d(nx, ny, nz)); //switching y and z for compatibility
			}
			else if (type == "f")
			{
				vector<int> vertexIndices;
				vector<int> normalIndices;
				string segment;

				// Parse indices ("1" "1/2" "1/2/3" "1//3")
				while (ss >> segment)
				{
					size_t slashPos1 = segment.find('/');
					int index, normal;
					if (slashPos1 == string::npos)
					{
						index = stoi(segment);
					}
					else
					{
						index = stoi(segment.substr(0, slashPos1));
						size_t slashPos2 = segment.find('/', slashPos1 + 1);
						if (slashPos2 != string::npos)
						{
							normal = stoi(segment.substr(slashPos2 + 1));
						}
					}

					vertexIndices.push_back(index - 1);
					normalIndices.push_back(normal - 1);
				}

				// Triangulate the face (any n gon works)
				// Creates a triangle fan: (0, 1, 2), (0, 2, 3), etc.
				for (size_t i = 1; i < vertexIndices.size() - 1; ++i)
				{
					TriangleToRender tri;

					point3d p1Raw = tempVertices[vertexIndices[0]];
					point3d p2Raw = tempVertices[vertexIndices[i]];
					point3d p3Raw = tempVertices[vertexIndices[i + 1]];

					tri.p1 = transform(p1Raw, scale, x, y, z);
					tri.p2 = transform(p2Raw, scale, x, y, z);
					tri.p3 = transform(p3Raw, scale, x, y, z);

					tri.symbol = symbol;

					if (Normals.size() > 0)
					{
						tri.n1 = Normals[normalIndices[0]];
						tri.n2 = Normals[normalIndices[i]];
						tri.n3 = Normals[normalIndices[i + 1]];
						tri.setAvgNormal();
					}

					meshTriangles.push_back(tri);
				}
			}
		}
		file.close();
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
	
	if (x1 == x2 && y1 == y2) return;

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
	if      (c == 'x') return round(focalLengthpx * (pointa.x / pointa.y)); //col
	else if (c == 'y') return round(focalLengthpx * (pointa.z / pointa.y)); //row
	return -1;
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


	angleRad = toradian(camera.pitch - 90);
	cosangleRad = cos(angleRad), sinangleRad = sin(angleRad);

	float pitchedy = yawedy * cosangleRad - newz * sinangleRad;
	float pitchedz = yawedy * sinangleRad + newz * cosangleRad;

	return point3d(yawedx, pitchedy, pitchedz);
}
point3d transformtoWorldSpace(const point3d& w)
{
	float angleRad = toradian(-camera.pitch + 90);
	float cosangleRad = cos(angleRad), sinangleRad = sin(angleRad);

	float pitchedy = w.y * cosangleRad - w.z * sinangleRad;
	float pitchedz = w.y * sinangleRad + w.z * cosangleRad;


	angleRad = toradian(-camera.yaw);
	cosangleRad = cos(angleRad), sinangleRad = sin(angleRad);

	float yawedx = w.x * cosangleRad - pitchedy * sinangleRad;
	float yawedy = w.x * sinangleRad + pitchedy * cosangleRad;

	return point3d(yawedx + camera.x, yawedy + camera.y, pitchedz + camera.z);
}

void renderTriangle3d(const TriangleToRender& tri)
{
	float near_plane = 0.1f;
	if (tri.p1.y < near_plane || tri.p2.y < near_plane || tri.p3.y < near_plane)
		return;

	point point1(project3d(tri.p1, 'x'), project3d(tri.p1, 'y'));
	point point2(project3d(tri.p2, 'x'), project3d(tri.p2, 'y'));
	point point3(project3d(tri.p3, 'x'), project3d(tri.p3, 'y'));

	point3d lightDirinv = lightdir * -1;

	float brightness = (tri.avgNor.normalized() * lightDirinv.normalized() + 1)/2; // 0 to 1
	int brightnessIndex = round(brightness * (brightnessSymbols.length() - 1));

	printTriangle(point1, point2, point3, brightnessSymbols[brightnessIndex]);
}

void addTriangle3d(point3d pointa, point3d pointb, point3d pointc, char c = ch, point3d avgNor = {0, 0, 1})
{
	TriangleToRender tri = {pointa, pointb, pointc, c, avgNor};
	queue.push_back(tri);
}
void addQuad3d(point3d pointa, point3d pointb, point3d pointc, point3d pointd, char c = ch, point3d avgNor = { 0, 0, 1 })
{
	TriangleToRender tri1 = { pointa, pointb, pointc, c, avgNor }, tri2 = { pointa, pointc, pointd, c, avgNor };
	queue.push_back(tri1);
	queue.push_back(tri2);
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
			addQuad3d(e, f, g, h, '*', {0, 1, 0}); // +y back
		}
		if (!topmatched)
		{
			addQuad3d(a, b, f, e, '.', { 0, 0, 1 });     //-z bottom
		}
		if (!rightmatched)
		{
			addQuad3d(a, d, h, e, '#', { -1, 0, 0 });    //-x left
		}
		if (!leftmatched)
		{
			addQuad3d(b, c, g, f, '#', { 1, 0, 0 });    //+x right
		}
		if (!bottommatched)
		{
			addQuad3d(c, d, h, g, '@', { 0, 0, 1 }); // +z top
		}
		if (!backmatched)
		{
			addQuad3d(a, b, c, d, '*', { 0, -1, 0 });    //-y front
		}
	}
};
vector<block> worldBlocks;

void renderEquations()
{
	size_t size = Equations.size();
	vector<bool>prevSigns(size), currentSigns(size);
	
	chrono::duration<float> totalElapsed = (currentTime - startTime);
	float totalElapsedSec = totalElapsed.count();

	//heartRef->scale = 2.5 + exp(sin(totalElapsedSec * 6)/4);


	point3d startPos = {camera.x, camera.y, camera.z};

	point3d relative00PxPos = {- float(col - 1)/2, focalLengthpx,float(row - 1)/2};
	point3d relative10PxPos = {0 - float(col - 1)/2, focalLengthpx,-1 + float(row - 1)/2};
	point3d relative01PxPos = {1 - float(col - 1)/2, focalLengthpx, float(row - 1)/2};


	float pitchRad = toradian(-camera.pitch + 90), yawRad = toradian(-camera.yaw);
	float cosPitch = cos(pitchRad), sinPitch = sin(pitchRad);
	float cosYaw = cos(yawRad), sinYaw = sin(yawRad);

	float pitchedy00 = relative00PxPos.y * cosPitch - relative00PxPos.z * sinPitch;
	float pitchedz00 = relative00PxPos.y * sinPitch + relative00PxPos.z * cosPitch;

	float pitchedy01 = relative01PxPos.y * cosPitch - relative01PxPos.z * sinPitch;
	float pitchedz01 = relative01PxPos.y * sinPitch + relative01PxPos.z * cosPitch;

	float pitchedy10 = relative10PxPos.y * cosPitch - relative10PxPos.z * sinPitch;
	float pitchedz10 = relative10PxPos.y * sinPitch + relative10PxPos.z * cosPitch;


	float yawedx00 = relative00PxPos.x * cosYaw - pitchedy00 * sinYaw;
	float yawedy00 = relative00PxPos.x * sinYaw + pitchedy00 * cosYaw;

	float yawedx01 = relative01PxPos.x * cosYaw - pitchedy01 * sinYaw;
	float yawedy01 = relative01PxPos.x * sinYaw + pitchedy01 * cosYaw;

	float yawedx10 = relative10PxPos.x * cosYaw - pitchedy10 * sinYaw;
	float yawedy10 = relative10PxPos.x * sinYaw + pitchedy10 * cosYaw;


	point3d _00PxPos = {yawedx00 + camera.x, yawedy00 + camera.y, pitchedz00 + camera.z};
	point3d _01PxPos = {yawedx01 + camera.x, yawedy01 + camera.y, pitchedz01 + camera.z};
	point3d _10PxPos = {yawedx10 + camera.x, yawedy10 + camera.y, pitchedz10 + camera.z};

	point3d leftRight1px = _01PxPos - _00PxPos;
	point3d upDown1px = _10PxPos - _00PxPos;

	vector<vector<point3d>> pixelDirections(row, vector<point3d>(col));
	for (int i = 0; i < row; i++)
	{
		for (int j = 0; j < col; j++)
		{
			if (j == 0)
				if(i == 0)
					pixelDirections[i][j] = _00PxPos - startPos;
				else
					pixelDirections[i][j] = pixelDirections[i - 1][j] + upDown1px;
			else
				pixelDirections[i][j] = pixelDirections[i][j - 1] + leftRight1px;
		}
	}

	for (int i = 0; i < row; i++)
	{
		for (int j = 0; j < col; j++)
		{	
			point3d unitVec = pixelDirections[i][j].normalized();

			int eqMatched = 0;

			point3d currentPos = startPos;
			
			for (int it = 0; it < size; it++)
			{
				prevSigns[it] = Equations[it]->evaluate(currentPos) <= 0;
			}

			float currentEquationRayStep = equationRayStep;
			for (float d = 0; d <= renderDistance; d += currentEquationRayStep/*, currentEquationRayStep *= 1.1*/)
			{
				bool matched = 0;

				currentPos = currentPos + unitVec * currentEquationRayStep;

				for (int it = 0; it < size; it++)
				{
					float solution = Equations[it]->evaluate(currentPos);
					currentSigns[it] = solution <= 0;
					if (currentSigns[it] != prevSigns[it] && abs(solution) != FLT_MAX)
					{
						matched = true;

						point3d camPos = {camera.x, camera.y, camera.z};
						point3d normal = Equations[it]->gradient(camPos + (unitVec * d));

						float brightness = (((lightdir * -1) * normal) + 1)/2;
						brightness = brightness > 1? 1: (brightness < 0? 0 : brightness);

						debugbrightness = brightness;
						int brightnessIndex = round(brightness * (brightnessSymbols.length() - 1));

						screenSet(j, i, brightnessSymbols[brightnessIndex]);
						break;
					}
				}
				if(matched)break;
				prevSigns = currentSigns;
			}
		}
	}
}
void renderEquations2()
{
	size_t size = Equations.size();

	chrono::duration<float> totalElapsed = (currentTime - startTime);
	float totalElapsedSec = totalElapsed.count();

	//heartRef->scale = 2.5 + exp(sin(totalElapsedSec * 6)/4);

	point3d startPos = {camera.x, camera.y, camera.z};

	point3d relative00PxPos = {- float(col - 1)/2, focalLengthpx,float(row - 1)/2};
	point3d relative10PxPos = {0 - float(col - 1)/2, focalLengthpx,-1 + float(row - 1)/2};
	point3d relative01PxPos = {1 - float(col - 1)/2, focalLengthpx, float(row - 1)/2};


	float pitchRad = toradian(-camera.pitch + 90), yawRad = toradian(-camera.yaw);
	float cosPitch = cos(pitchRad), sinPitch = sin(pitchRad);
	float cosYaw = cos(yawRad), sinYaw = sin(yawRad);

	float pitchedy00 = relative00PxPos.y * cosPitch - relative00PxPos.z * sinPitch;
	float pitchedz00 = relative00PxPos.y * sinPitch + relative00PxPos.z * cosPitch;

	float pitchedy01 = relative01PxPos.y * cosPitch - relative01PxPos.z * sinPitch;
	float pitchedz01 = relative01PxPos.y * sinPitch + relative01PxPos.z * cosPitch;

	float pitchedy10 = relative10PxPos.y * cosPitch - relative10PxPos.z * sinPitch;
	float pitchedz10 = relative10PxPos.y * sinPitch + relative10PxPos.z * cosPitch;


	float yawedx00 = relative00PxPos.x * cosYaw - pitchedy00 * sinYaw;
	float yawedy00 = relative00PxPos.x * sinYaw + pitchedy00 * cosYaw;

	float yawedx01 = relative01PxPos.x * cosYaw - pitchedy01 * sinYaw;
	float yawedy01 = relative01PxPos.x * sinYaw + pitchedy01 * cosYaw;

	float yawedx10 = relative10PxPos.x * cosYaw - pitchedy10 * sinYaw;
	float yawedy10 = relative10PxPos.x * sinYaw + pitchedy10 * cosYaw;


	point3d _00PxPos = {yawedx00 + camera.x, yawedy00 + camera.y, pitchedz00 + camera.z};
	point3d _01PxPos = {yawedx01 + camera.x, yawedy01 + camera.y, pitchedz01 + camera.z};
	point3d _10PxPos = {yawedx10 + camera.x, yawedy10 + camera.y, pitchedz10 + camera.z};

	point3d leftRight1px = _01PxPos - _00PxPos;
	point3d upDown1px = _10PxPos - _00PxPos;

	vector<vector<point3d>> pixelDirections(row, vector<point3d>(col));
	for (int i = 0; i < row; i++)
	{
		for (int j = 0; j < col; j++)
		{
			if (j == 0)
				if(i == 0)
					pixelDirections[i][j] = _00PxPos - startPos;
				else
					pixelDirections[i][j] = pixelDirections[i - 1][j] + upDown1px;
			else
				pixelDirections[i][j] = pixelDirections[i][j - 1] + leftRight1px;
		}
	}

	vector<vector<float>> zbuffer(row, vector<float>(col, FLT_MAX));
	vector<vector<int>> equationIndex(row, vector<int>(col, -1));

	for(int it = 0; it < size; it++)
	{
		for (int i = 0; i < row; i++)
		{
			for (int j = 0; j < col; j++)
			{
				point3d currentPos = startPos;

				bool prevSign = Equations[it]->evaluate(currentPos) <= 0;
				bool currentSign;

				float currentEquationRayStep = equationRayStep;
				for (float d = 0; d <= renderDistance && d <= zbuffer[i][j]; d += currentEquationRayStep/*, currentEquationRayStep *= 1.1*/)
				{
					currentPos = currentPos + pixelDirections[i][j].normalized() * currentEquationRayStep;

					float solution = Equations[it]->evaluate(currentPos);
					currentSign = solution <= 0;

					if (currentSign != prevSign && abs(solution) != FLT_MAX)
					{
						if(d < zbuffer[i][j])
						{
							zbuffer[i][j] = d;
							equationIndex[i][j] = it;
						}
						break;
					}
					prevSign = currentSign;
				}
			}
		}
	}
	for (int i = 0; i < row; i++)
	{
		for (int j = 0; j < col; j++)
		{
			if(equationIndex[i][j] == -1) continue;

			point3d normal = Equations[equationIndex[i][j]]->gradient(startPos + (pixelDirections[i][j].normalized() * zbuffer[i][j]));

			float brightness = (((lightdir * -1) * normal) + 1)/2;
			brightness = brightness > 1? 1: (brightness < 0? 0 : brightness);

			debugbrightness = brightness;
			int brightnessIndex = round(brightness * (brightnessSymbols.length() - 1));

			screenSet(j, i, brightnessSymbols[brightnessIndex]);
		}
	}
}

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

	renderEquations();
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
		outFile << defaultSettingsText;
		outFile.close();
	}

	filename = "Models Positions.txt";
	if (!fileExists(filename))
	{
		ofstream outFile(filename);
		outFile << defaultModelspositionsText;
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
		catch (const invalid_argument& e)
		{
			cerr << "Warning: Corrupted or invalid data in save file for key: " << key << endl;
		}
		catch (const out_of_range& e)
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
				hfov = stof(value);
			else if (key == "row")
				row = stoi(value);
			else if (key == "col")
				col = stoi(value) + 2; //adjusting the desired column number to actual column number as 2 colums on the sides are not displayed
			else if (key == "use_ncurses")
				useNcurses = stoi(value);
			else if (key == "cam_speed_mouse")
				camSpeedMouse = stof(value);
			else if (key == "cam_speed_keyboard")
				camSpeedKeyboard = stof(value);
			else if (key == "walk_speed")
				walkSpeed = stof(value);
		}
		catch (const invalid_argument& e)
		{
			cerr << "Warning: Corrupted or invalid data in settings file for key: " << key << endl;
		}
		catch (const out_of_range& e)
		{
			cerr << "Warning: Value out of range in settings file for key: " << key << endl;
		}
	}
	settings.close();
}
void loadModels()
{
	string line;
	ifstream models;
	models.open("Models Positions.txt");
	while (getline(models, line))
	{
		if (line.empty()) continue;
		if (line.back() == '\r')
		{
			line.pop_back();
		}
		if (line[0] == '#')
		{
			continue;
		}

		stringstream ss(line);

		string name;
		float x, y, z, scale;
		char symbol;

		ss >> name >> x >> y >> z >> scale >> symbol;

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

#if HAS_NCURSES
	if (useNcurses)
	{
		clear();

		for (int i = 0; i < col * 2 - 3; i++)
		{
			mvaddch(0, i, ACS_HLINE);
			mvaddch(row + 1, i, ACS_HLINE);
		}

		for (int i = 1; i <= row; i++)
		{
			mvaddch(i, 0, ACS_VLINE);
			mvaddch(i, col * 2 - 3, ACS_VLINE);
		}

		for (int i = 0; i < row; i++)
		{
			int term_x = 1;
			for (int j = 1; j < col - 1; j++)
			{
				if (i >= LINES || term_x >= COLS) break;

				char ch = screen[i][j];
				mvaddch(i + 1, term_x, ch);

				if(j != col - 2)
					term_x += 2;
			}
		}

		if (row < LINES)
		{
			mvprintw(row + 2, 0, "Position: %.2f %.2f %.2f  Yaw: %.1f Pitch: %.1f  FPS: %.1f  Brightness: %f",
				camera.x, camera.y, camera.z, camera.yaw, camera.pitch, (deltaTime > 0.0f ? 1.0f / deltaTime : 0.0f), debugbrightness);
		}

		refresh();
	}
#endif
	if (!(HAS_NCURSES && useNcurses))
	{
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

		cout << fullFrame;
		cout << "\nPosition: " << camera.x << " " << camera.y << " " << camera.z << " Yaw: " << camera.yaw << " Pitch: " << camera.pitch << " FPS: " << 1/deltaTime
			<< "                                        \n";
	}

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

void loadBuiltinBlocks()
{
	/*placeHouse(-5, -5, 0);
	placeHouse(-18, -5, 0);

	tree(-24, -7, 0);*/
	/*addTriangle3d({ 0, 0, 0 }, { 1, 0, 1 }, { 1, 0, 0 }, '@');*/
	showHello(-12, 12, 0);
}
void loadEquations()
{
	auto Sphere = make_unique<sphere>();
	Sphere->pos = {0, 0, 0};
	Equations.push_back(move(Sphere));

	auto Torus = make_unique<torus>();
	Torus->pos = {1, 1, 0};
	Equations.push_back(move(Torus));

	auto Heart = make_unique<heart>();
	Heart->pos = {2, -3, 0};
	Heart->scale = 2.5;
	heartRef = Heart.get();
	Equations.push_back(move(Heart));

	//auto Ripples = make_unique<ripples>();
	//Ripples->pos = {2, -3, 0};
	//Ripples->scale = 1;
	//Equations.push_back(move(Ripples));

	//auto line = make_unique<line2D>();
	//Equations.push_back(move(line));
}
void prepareWorld()
{
	//loadModels();
	loadBuiltinBlocks();
	loadEquations();
}

void action()
{
	for(auto &i: isPressed)
	{
		i.second = (GetAsyncKeyState(i.first) & 0x8000);
	}
	camera.positionMove();
	camera.directionMove();
	
	if(justReleased('E') || justReleased(VK_RBUTTON))
		placeBlock();
	else if(justReleased('Q') || justReleased(VK_LBUTTON))
		breakBlock();
	else if (justReleased('R'))
		system("cls");
	else if (justReleased('O'))
		save();
	else if (justReleased(VK_ESCAPE))
	{
		mouseLocked = !mouseLocked;
		//ShowCursor(!mouseLocked);
	}

	wasPressed = isPressed;
}

int main()
{
	ios_base::sync_with_stdio(false);
	cin.tie(NULL);

	load();
	prepareWorld();
	constructScreen();

	cout << "Press Any Key to start. Make sure to resize your terminal before starting.";
	while(1) if(_kbhit()) break;

#if HAS_NCURSES
	if(useNcurses)
	{
		// 1. Initialize curses in non-blocking mode
		initscr();
		cbreak();
		noecho();
		keypad(stdscr, TRUE);
		timeout(0);
		curs_set(0);

		// 2. Enable mouse position tracking (not used but it helps make the terminal non selectable)
		mousemask(ALL_MOUSE_EVENTS | BUTTON_MOVED, NULL);

		row = LINES - 3;
		col = COLS/2;
		constructScreen();
	}
#endif

	int screen_width = GetSystemMetrics(SM_CXSCREEN);
	int screen_height = GetSystemMetrics(SM_CYSCREEN);
	screenCenter.x = screen_width / 2;
	screenCenter.y = screen_height / 2;

	// Lock mouse initially
	SetCursorPos(screenCenter.x, screenCenter.y);

	auto lastTime = chrono::high_resolution_clock::now();
	startTime = lastTime;
	while (1)
	{
		currentTime = chrono::high_resolution_clock::now();
		chrono::duration<float> elapsedTime = currentTime - lastTime;
		deltaTime = elapsedTime.count();
		lastTime = currentTime;

		queue.clear();
		render();
		show();
		action();
	}
	return 0;
}