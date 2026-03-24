#include <windows.h>   
#include <vector>      
#include <string>      
#include <fstream>     
#include <sstream>     
#include <cmath>       
#include <algorithm>   

// Tamaño de la ventana y del buffer de dibujo
const int WIDTH = 900;
const int HEIGHT = 700;

// Creamos la estructura para manejar los vectores en 3D
struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {} // Constructor vacío
    Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {} // Constructor con valores
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); } // Suma de vectores
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); } // Resta de vectores
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); } // Multiplicación por escalar
};

// Creamos la estructura para manejar colores RGB
struct Color3 {
    float r, g, b;
    Color3() : r(0), g(0), b(0) {} // Constructor vacío
    Color3(float R, float G, float B) : r(R), g(G), b(B) {} // Constructor con valores
    Color3 operator+(const Color3& o) const { return Color3(r + o.r, g + o.g, b + o.b); } // Suma de colores
};

// Generamos el producto punto entre dos vectores
static inline float Dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

// Generamos el producto cruz entre dos vectores
static inline Vec3 Cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}

// Genramos la longitud de un vector
static inline float Length(const Vec3& v) { return std::sqrt(Dot(v, v)); }

// Convertimos un vector a longitud 1
static inline Vec3 Normalize(const Vec3& v) {
    float len = Length(v);
    if (len < 1e-8f) return Vec3(0,0,0); 
    return v * (1.0f / len);
}

// Limitamos un valor entre 0 y 1
static inline float Clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

// Convertimos el color RGB a entero 
static inline unsigned int PackColor(const Color3& c) {
    unsigned char r = (unsigned char)(Clamp01(c.r) * 255.0f);
    unsigned char g = (unsigned char)(Clamp01(c.g) * 255.0f);
    unsigned char b = (unsigned char)(Clamp01(c.b) * 255.0f);
    return (r) | (g << 8) | (b << 16);
}

// Se guarda una cara triangular usando índices de vértices
struct Face {
    int a, b, c;
    int faceColorIndex;
};

// Se guarda la información de una cámara
struct Camera {
    Vec3 eye;    
    Vec3 target; 
    Vec3 up;     
};

// Se guarda la información de una luz
struct Light {
    bool enabled;    
    Vec3 dir;        
    Color3 intensity; 
};

// Se guarda un vértice ya proyectado a pantalla
struct VertexScreen {
    float x, y, z; 
    Vec3 world;    
};

// Lista de vértices del modelo cargado
static std::vector<Vec3> g_vertices;

// Lista de caras del modelo cargado
static std::vector<Face> g_faces;

// Buffer principal de color
static unsigned int g_colorBuffer[HEIGHT][WIDTH];

// Buffer para la imagen filtrada
static unsigned int g_postBuffer[HEIGHT][WIDTH];

// Buffer de profundidad para z-buffer
static float g_depthBuffer[HEIGHT][WIDTH];

// Variables globales de estado
static bool g_usePerspective = true;   
static bool g_useFilter = false;       
static bool g_rotate = false;          
static bool g_useTrianglesOBJ = true;  
static int g_cameraIndex = 0;          
static float g_yaw = 0.8f;             
static float g_pitch = -0.3f;          
static float g_autoAngle = 0.0f;       

// Creamos las dos cámaras para el cubo
static Camera g_cameras[2] = {
    { Vec3(45.0f, 30.0f, 45.0f), Vec3(0,0,0), Vec3(0,1,0) },
    { Vec3(-50.0f, 25.0f, 35.0f), Vec3(0,0,0), Vec3(0,1,0) }
};

// Generamos las luces blanca y roja
static Light g_lights[2] = {
    { true, Normalize(Vec3(-1.0f, -1.0f, -1.0f)), Color3(0.8f, 0.8f, 0.8f) },
    { true, Normalize(Vec3(-0.2f, -0.3f, -1.0f)), Color3(0.8f, 0.0f, 0.0f) }
};

// Colores de las caras del cubo
static Color3 g_faceColors[6] = {
    Color3(1.0f, 0.2f, 0.2f),
    Color3(0.2f, 1.0f, 0.2f),
    Color3(0.2f, 0.2f, 1.0f),
    Color3(1.0f, 1.0f, 0.2f),
    Color3(1.0f, 0.2f, 1.0f),
    Color3(0.2f, 1.0f, 1.0f)
};

// Coeficientes del material para iluminación
static Color3 g_ka(0.3f, 0.3f, 0.3f);             
static Color3 g_kd(0.07568f, 0.61424f, 0.07568f); 
static Color3 g_ks(0.633f, 0.727811f, 0.633f);    
static float g_shininess = 76.8f;                 

// Rotación de un vector alrededor del eje X
Vec3 RotateX(const Vec3& v, float a) {
    float c = std::cos(a), s = std::sin(a);
    return Vec3(v.x, v.y*c - v.z*s, v.y*s + v.z*c);
}

// Rotación de un vector alrededor del eje Y
Vec3 RotateY(const Vec3& v, float a) {
    float c = std::cos(a), s = std::sin(a);
    return Vec3(v.x*c + v.z*s, v.y, -v.x*s + v.z*c);
}

// Cargamos el archivo OBJ y guardamos los vértices y caras
bool LoadOBJ(const char* filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false; 
    g_vertices.clear(); 
    g_faces.clear();    
    std::string line;
    int faceCounter = 0;
    while (std::getline(file, line)) {
        if (line.size() < 2) continue; 
        std::stringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "v") { 
            float x, y, z;
            ss >> x >> y >> z;
            g_vertices.push_back(Vec3(x, y, z));
        } else if (tag == "f") { 
            std::vector<int> idx;
            std::string token;
            while (ss >> token) {
                size_t slash = token.find('/');
                if (slash != std::string::npos) token = token.substr(0, slash); 
                idx.push_back(std::stoi(token) - 1); 
            }
            int colorIndex = faceCounter % 6;
            if (idx.size() == 3) { 
                g_faces.push_back({ idx[0], idx[1], idx[2], colorIndex });
                faceCounter++;
            } else if (idx.size() == 4) { 
                g_faces.push_back({ idx[0], idx[1], idx[2], colorIndex });
                g_faces.push_back({ idx[0], idx[2], idx[3], colorIndex });
                faceCounter++;
            }
        }
    }
    return !g_vertices.empty() && !g_faces.empty(); 
}

// Limpiamos los buffers para dibujar un nuevo frame
void ClearBuffers() {
    unsigned int bg = PackColor(Color3(0.08f, 0.08f, 0.10f)); 
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {
            g_colorBuffer[y][x] = bg;    
            g_depthBuffer[y][x] = 1e30f; 
        }
    }
}

// Crea un punto del mundo al espacio de la cámara
Vec3 ViewTransform(const Vec3& p, const Camera& cam) {
    Vec3 f = Normalize(cam.target - cam.eye); 
    Vec3 r = Normalize(Cross(f, cam.up));     
    Vec3 u = Cross(r, f);                     
    Vec3 q = p - cam.eye;                     
    return Vec3(Dot(q, r), Dot(q, u), Dot(q, f));
}

// Proyecta un punto 3D a coordenadas 2D
bool ProjectPoint(const Vec3& camP, float& sx, float& sy, float& sz) {
    if (g_usePerspective) { 
        if (camP.z <= 0.01f) return false; 
        float f =700.0f;
        sx = (camP.x / camP.z) * f + WIDTH * 0.5f;
        sy = (-camP.y / camP.z) * f + HEIGHT * 0.5f;
        sz = camP.z;
        return true;
    } else { 
        float scale = 12.0f;
        sx = camP.x * scale + WIDTH * 0.5f;
        sy = -camP.y * scale + HEIGHT * 0.5f;
        sz = camP.z;
        return true;
    }
}

// Calculamos el color final de un píxel con Blinn-Phong
Color3 ShadePixel(const Vec3& P, const Vec3& N, const Color3& faceColor, const Camera& cam) {
    Vec3 normal = Normalize(N);         
    Vec3 V = Normalize(cam.eye - P);    
    Color3 result(faceColor.r * g_ka.r, faceColor.g * g_ka.g, faceColor.b * g_ka.b); 

    for (int i = 0; i < 2; ++i) {
        if (!g_lights[i].enabled) continue; 

        Vec3 L = Normalize(g_lights[i].dir * -1.0f); 
        float ndotl = std::max(0.0f, Dot(normal, L)); 
        Vec3 H = Normalize(L + V);                    
        float ndoth = std::max(0.0f, Dot(normal, H)); 
        float spec = std::pow(ndoth, g_shininess);    
        result = result + Color3(
            faceColor.r * g_kd.r * g_lights[i].intensity.r * ndotl + g_ks.r * g_lights[i].intensity.r * spec,
            faceColor.g * g_kd.g * g_lights[i].intensity.g * ndotl + g_ks.g * g_lights[i].intensity.g * spec,
            faceColor.b * g_kd.b * g_lights[i].intensity.b * ndotl + g_ks.b * g_lights[i].intensity.b * spec
        );
    }
    return Color3(Clamp01(result.r), Clamp01(result.g), Clamp01(result.b)); 
}

// Creamos una función auxiliar para rasterizar triángulos con edge function
float Edge(float ax, float ay, float bx, float by, float px, float py) {
    return (px - ax)*(by - ay) - (py - ay)*(bx - ax);
}

// Ahora dibujamos un triángulo rellenándolo
void DrawTriangle(const VertexScreen& v0, const VertexScreen& v1, const VertexScreen& v2,
                  const Vec3& faceNormal, const Color3& faceColor, const Camera& cam) {
    float minX = std::max(0.0f, std::floor(std::min(v0.x, std::min(v1.x, v2.x))));
    float maxX = std::min((float)WIDTH - 1, std::ceil(std::max(v0.x, std::max(v1.x, v2.x))));
    float minY = std::max(0.0f, std::floor(std::min(v0.y, std::min(v1.y, v2.y))));
    float maxY = std::min((float)HEIGHT - 1, std::ceil(std::max(v0.y, std::max(v1.y, v2.y))));

    float area = Edge(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y);
    if (std::fabs(area) < 1e-8f) return; 

    for (int y = (int)minY; y <= (int)maxY; ++y) {
        for (int x = (int)minX; x <= (int)maxX; ++x) {
            float px = x + 0.5f;
            float py = y + 0.5f;
            float w0 = Edge(v1.x, v1.y, v2.x, v2.y, px, py);
            float w1 = Edge(v2.x, v2.y, v0.x, v0.y, px, py);
            float w2 = Edge(v0.x, v0.y, v1.x, v1.y, px, py);
            bool inside = (area > 0) ? (w0 >= 0 && w1 >= 0 && w2 >= 0) : (w0 <= 0 && w1 <= 0 && w2 <= 0);
            if (!inside) continue; 
            w0 /= area; w1 /= area; w2 /= area;
            float z = v0.z*w0 + v1.z*w1 + v2.z*w2; 
            if (z < g_depthBuffer[y][x]) { 
                g_depthBuffer[y][x] = z;
                Vec3 P = v0.world*w0 + v1.world*w1 + v2.world*w2; 
                g_colorBuffer[y][x] = PackColor(ShadePixel(P, faceNormal, faceColor, cam)); 
            }
        }
    }
}

// Aplicamos un filtro kernel a la imagen final
void ApplyKernel() {
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x)
            g_postBuffer[y][x] = g_colorBuffer[y][x]; 
    for (int y = 1; y < HEIGHT - 1; ++y) {
        for (int x = 1; x < WIDTH - 1; ++x) {
            float sumR = 0, sumG = 0, sumB = 0;
            for (int ky = -1; ky <= 1; ++ky) {
                for (int kx = -1; kx <= 1; ++kx) {
                    unsigned int c = g_colorBuffer[y + ky][x + kx];
                    sumR += ((c) & 255) / 255.0f;
                    sumG += ((c >> 8) & 255) / 255.0f;
                    sumB += ((c >> 16) & 255) / 255.0f;
                }
            }
            unsigned int c = g_colorBuffer[y][x];
            float cr = ((c) & 255) / 255.0f;
            float cg = ((c >> 8) & 255) / 255.0f;
            float cb = ((c >> 16) & 255) / 255.0f;
            g_postBuffer[y][x] = PackColor(Color3(
                Clamp01(2.0f * cr - sumR / 9.0f),
                Clamp01(2.0f * cg - sumG / 9.0f),
                Clamp01(2.0f * cb - sumB / 9.0f)
            ));
        }
    }
}

// Renderizamos la escena
void RenderScene() {
    ClearBuffers(); 
    Camera cam = g_cameras[g_cameraIndex]; 
    std::vector<Vec3> worldVerts(g_vertices.size());           
    std::vector<VertexScreen> scrVerts(g_vertices.size());     
    float ay = g_yaw + (g_rotate ? g_autoAngle : 0.0f); 
    float ax = g_pitch;                                 
    for (size_t i = 0; i < g_vertices.size(); ++i) {
        Vec3 p = RotateX(RotateY(g_vertices[i], ay), ax); 
        worldVerts[i] = p;
        Vec3 cp = ViewTransform(p, cam); 
        float sx, sy, sz;
        if (!ProjectPoint(cp, sx, sy, sz))
            scrVerts[i] = { -99999, -99999, 1e30f, p }; 
        else
            scrVerts[i] = { sx, sy, sz, p }; 
    }
    for (const Face& f : g_faces) {
        Vec3 p0 = worldVerts[f.a];
        Vec3 p1 = worldVerts[f.b];
        Vec3 p2 = worldVerts[f.c];
        Vec3 normal = Normalize(Cross(p1 - p0, p2 - p0)); 
        Vec3 viewDir = Normalize(cam.eye - p0);           
        if (Dot(normal, viewDir) <= 0.0f) continue;       
        const VertexScreen& s0 = scrVerts[f.a];
        const VertexScreen& s1 = scrVerts[f.b];
        const VertexScreen& s2 = scrVerts[f.c];
        if (s0.z > 1e20f || s1.z > 1e20f || s2.z > 1e20f) continue; 
        DrawTriangle(s0, s1, s2, normal, g_faceColors[f.faceColorIndex % 6], cam); 
    }
    if (g_useFilter) ApplyKernel(); 
}

// Dibujamos textos en la pantalla para que sea mas facil la visualización de cada cosa
void DrawTextInfo(HDC hdc) {
    SetBkMode(hdc, TRANSPARENT);         
    SetTextColor(hdc, RGB(230,230,230)); 

    std::string t1 = "1: Luz blanca  2: Luz roja  L: Solo luz roja  C: Camara  P: Proyeccion";
    std::string t2 = "F: Filtro  O: Cambiar OBJ  ESPACIO: Rotacion  Flechas: Girar  R: Reiniciar  ESC: Salir";
    std::string t3 = std::string("OBJ: ") + (g_useTrianglesOBJ ? "Cube_Triangles.obj" : "Cube_Quads.obj")
        + " | Camara: " + (g_cameraIndex == 0 ? "1" : "2")
        + " | Proyeccion: " + (g_usePerspective ? "Perspectiva" : "Ortogonal");
    TextOutA(hdc, 10, 10, t1.c_str(), (int)t1.size()); 
    TextOutA(hdc, 10, 30, t2.c_str(), (int)t2.size()); 
    TextOutA(hdc, 10, 50, t3.c_str(), (int)t3.size()); 
}

// Es el procedimiento principal 
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_KEYDOWN: 
        switch (wParam) {
        case '1': g_lights[0].enabled = !g_lights[0].enabled; InvalidateRect(hwnd, NULL, FALSE); break; // Activa/desactiva luz blanca
        case '2': g_lights[1].enabled = !g_lights[1].enabled; InvalidateRect(hwnd, NULL, FALSE); break; // Activa/desactiva luz roja
        case 'C': g_cameraIndex = 1 - g_cameraIndex; InvalidateRect(hwnd, NULL, FALSE); break;          // Cambia cámara
        case 'P': g_usePerspective = !g_usePerspective; InvalidateRect(hwnd, NULL, FALSE); break;       // Cambia proyección
        case 'F': g_useFilter = !g_useFilter; InvalidateRect(hwnd, NULL, FALSE); break;                 // Activa filtro
        case 'O':
            g_useTrianglesOBJ = !g_useTrianglesOBJ; 
            LoadOBJ(g_useTrianglesOBJ ? "Cube_Triangles.obj" : "Cube_Quads.obj");
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case 'L':
            g_lights[0].enabled = false; 
            g_lights[1].enabled = true;  
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case 'R':
            g_yaw = 0.8f;              
            g_pitch = -0.3f;          
            g_autoAngle = 0.0f;        
            g_cameraIndex = 0;         
            g_usePerspective = true;   
            g_useFilter = false;       
            g_rotate = false;          
            g_lights[0].enabled = true; 
            g_lights[1].enabled = true; 
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        case VK_SPACE: g_rotate = !g_rotate; InvalidateRect(hwnd, NULL, FALSE); break; 
        case VK_LEFT:  g_yaw -= 0.10f; InvalidateRect(hwnd, NULL, FALSE); break;      
        case VK_RIGHT: g_yaw += 0.10f; InvalidateRect(hwnd, NULL, FALSE); break;       
        case VK_UP:    g_pitch -= 0.10f; InvalidateRect(hwnd, NULL, FALSE); break;    
        case VK_DOWN:  g_pitch += 0.10f; InvalidateRect(hwnd, NULL, FALSE); break;    
        case VK_ESCAPE: DestroyWindow(hwnd); break;                                     
        }
        return 0;
    case WM_TIMER: 
        if (g_rotate) {
            g_autoAngle += 0.015f;           
            InvalidateRect(hwnd, NULL, FALSE); 
        }
        return 0;
    case WM_PAINT: { 
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RenderScene(); 
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = WIDTH;
        bmi.bmiHeader.biHeight = -HEIGHT; 
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        const void* pixels = g_useFilter ? (const void*)g_postBuffer : (const void*)g_colorBuffer;
        StretchDIBits(hdc, 0, 0, WIDTH, HEIGHT, 0, 0, WIDTH, HEIGHT, pixels, &bmi, DIB_RGB_COLORS, SRCCOPY); // Dibuja buffer

        DrawTextInfo(hdc); 
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY: 
        KillTimer(hwnd, 1);     
        PostQuitMessage(0);     
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam); 
}

// Función principal del programa
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShowCmd) {
    if (!LoadOBJ("Cube_Triangles.obj")) { //
        MessageBoxA(NULL, "No se pudo abrir Cube_Triangles.obj en la misma carpeta del ejecutable.", "Error", MB_ICONERROR);
        return 0;
    }

    WNDCLASSA wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;    
    wc.lpfnWndProc = WndProc;              
    wc.hInstance = hInst;                  
    wc.lpszClassName = "Tarea5WinAPIV2Class"; 
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); 
    RegisterClassA(&wc);                   
    HWND hwnd = CreateWindowA(
        wc.lpszClassName,                          
        "Tarea 5 - Cubo OBJ", 
        WS_OVERLAPPEDWINDOW,                      
        CW_USEDEFAULT, CW_USEDEFAULT,             
        WIDTH + 16, HEIGHT + 39,                  
        NULL, NULL, hInst, NULL
    );
    ShowWindow(hwnd, nShowCmd); 
    UpdateWindow(hwnd);         
    SetTimer(hwnd, 1, 16, NULL); 
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) { 
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}