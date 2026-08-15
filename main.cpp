#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <limits>

// وضعیت‌های برنامه
enum class AppState {
    MAIN_MENU,
    NEW_PROJECT_DIALOG,
    WORKSPACE,
    PICK_DEVICES_DIALOG // پنجره کتابخانه قطعات (بخش سوم)
};

// ساختار تعریف یک قطعه الکترونیکی (برای کتابخانه)
struct ComponentDef {
    std::string id;
    std::string name;
    std::string category;
    std::string description;
};

// ساختار پروژه
struct RecentProject {
    std::string name;
    std::string lastModified;
};

// ==========================================
// --- ساختارهای سیم‌کشی (بخش پنجم) ---
// ==========================================
struct Wire {
    int startComponent;
    int startPin;
    int endComponent;
    int endPin;
    std::vector<SDL_FPoint> points;

    Wire(int startComp, int startPinIndex, int endComp, int endPinIndex)
            : startComponent(startComp),
              startPin(startPinIndex),
              endComponent(endComp),
              endPin(endPinIndex) {}
};

struct Junction {
    float x;
    float y;
    bool active;

    Junction(float worldX = 0.0f, float worldY = 0.0f)
            : x(worldX), y(worldY), active(true) {}
};

// ==========================================
// --- ساختارهای بخش چهارم (مدیریت بوم) ---
// ==========================================
struct Pin {
    float relX, relY; // فاصله نسبی پایه از مرکز قطعه
};

class CircuitComponent {
public:
    std::string type;
    std::string name;
    std::string value;
    float x, y;              // مختصات جهانی مرکز قطعه
    float startX, startY;    // مختصات کمکی برای درگ کردن نرم
    float width, height;
    int rotation;
    bool isMirroredX;
    bool isMirroredY;
    bool isSelected;
    std::vector<Pin> pins;

    CircuitComponent(std::string compType, float startWorldX, float startWorldY, std::string defaultName) {
        type = compType;
        name = defaultName;
        x = startWorldX;
        y = startWorldY;
        width = 80;
        height = 40;
        rotation = 0;
        isMirroredX = false;
        isMirroredY = false;
        isSelected = false;

        // تنظیم شماتیک پایه‌ها بر اساس نوع قطعه
        if (type == "RES" || type == "CAP" || type == "IND" || type == "DIODE") {
            pins.push_back({-40, 0}); // پایه چپ
            pins.push_back({40, 0});  // پایه راست
            value = (type == "CAP") ? "1uF" : "10k";
        } else if (type == "DC_SRC" || type == "GND") {
            pins.push_back({0, -30}); // بالا
            pins.push_back({0, 30});  // پایین
            width = 60; height = 60;
            value = "5V";
        } else {
            pins.push_back({-30, -10});
            pins.push_back({-30, 10});
            pins.push_back({30, 0});
            width = 60; height = 60;
            value = "";
        }
    }

    // بررسی اینکه آیا مختصات (جهانی) موس روی قطعه است یا خیر
    bool contains(float wx, float wy) const {
        float currentW = (rotation % 180 == 0) ? width : height;
        float currentH = (rotation % 180 == 0) ? height : width;
        return (wx >= x - currentW / 2 && wx <= x + currentW / 2 &&
                wy >= y - currentH / 2 && wy <= y + currentH / 2);
    }

    // چرخش قطعه و پایه‌ها (آیتم 4.4)
    void rotate() {
        rotation = (rotation + 90) % 360;
        for (auto& pin : pins) {
            float oldX = pin.relX;
            pin.relX = -pin.relY;
            pin.relY = oldX;
        }
    }

    // قرینه‌سازی قطعه و پایه‌ها (آیتم 5.4)
    void mirror(bool horizontal) {
        if (horizontal) {
            isMirroredX = !isMirroredX;
            for (auto& pin : pins) pin.relX = -pin.relX;
        } else {
            isMirroredY = !isMirroredY;
            for (auto& pin : pins) pin.relY = -pin.relY;
        }
    }
};
// ==========================================


// کلاس دکمه
class Button {
public:
    SDL_Rect rect;
    SDL_Color bgColor;
    SDL_Color textColor;
    std::string text;
    SDL_Texture* textTexture;
    SDL_Rect textRect;

    Button(int x, int y, int w, int h, SDL_Color bg, SDL_Color txtColor, std::string t, TTF_Font* font, SDL_Renderer* renderer)
            : bgColor(bg), textColor(txtColor), text(t) {
        rect = {x, y, w, h};

        SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), textColor);
        if (surface) {
            textTexture = SDL_CreateTextureFromSurface(renderer, surface);
            textRect.w = surface->w;
            textRect.h = surface->h;
            textRect.x = rect.x + (rect.w - textRect.w) / 2;
            textRect.y = rect.y + (rect.h - textRect.h) / 2;
            SDL_FreeSurface(surface);
        } else {
            textTexture = nullptr;
        }
    }

    ~Button() {
        if (textTexture) SDL_DestroyTexture(textTexture);
    }

    void draw(SDL_Renderer* renderer) {
        SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, 255);
        SDL_RenderFillRect(renderer, &rect);
        if (textTexture) {
            SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
        }
    }

    bool isClicked(int mouseX, int mouseY) {
        return (mouseX >= rect.x && mouseX <= rect.x + rect.w &&
                mouseY >= rect.y && mouseY <= rect.y + rect.h);
    }
};

// کلاس اصلی مدیریت برنامه
class AppManager {
private:
    bool isRunning;
    AppState currentState;
    std::vector<RecentProject> recentProjects;
    TTF_Font* mainFont;
    TTF_Font* statusFont;
    TTF_Font* smallFont;

    Button* btnNewProject;
    Button* btnA4;
    Button* btnA3;
    Button* btnCustom;
    Button* btnBackToMenu;
    Button* btnPickDevices;
    Button* btnAddSelected;
    Button* btnCloseDialog;

    SDL_Texture* recentProjectsTitle;
    SDL_Rect titleRect;
    std::vector<SDL_Texture*> recentTextTextures;
    std::vector<SDL_Rect> recentTextRects;

    float cameraX, cameraY;
    float zoom;
    bool isPanning;
    int panStartX, panStartY;
    float camStartX, camStartY;
    int mouseX, mouseY;
    float worldX, worldY;
    float snappedWorldX, snappedWorldY;
    const float GRID_SIZE = 30.0f;

    std::vector<ComponentDef> library;
    std::vector<ComponentDef> activeDevices;
    int selectedActiveIndex;

    std::string searchQuery;
    std::string selectedCategory;
    int selectedLibraryIndex;

    const int SIDEBAR_WIDTH = 180;

    // ----- متغیرهای بخش چهارم (مدیریت بوم و قطعات) -----
    std::vector<CircuitComponent> canvasComponents;
    std::string deviceToPlace = "";
    int componentCounter = 1;

    bool isDragging = false;
    float dragStartWorldX = 0, dragStartWorldY = 0;

    bool isSelecting = false;
    float selectStartX = 0, selectStartY = 0;

    // ----- بخش پنجم: سیستم سیم‌کشی -----
    std::vector<Wire> wires;
    std::vector<Junction> junctions;

    bool isWiring = false;
    int wireStartComponent = -1;
    int wireStartPin = -1;

    int hoveredWireIndex = -1;
    int hoveredPinComponent = -1;
    int hoveredPinIndex = -1;

    CircuitComponent* propertiesTarget = nullptr;
    Uint32 lastClickTime = 0;
    // ----------------------------------------------------

    TTF_Font* loadFont(int fontSize) {
        TTF_Font* font = TTF_OpenFont("font.ttf", fontSize);
        if (!font) font = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", fontSize);
        if (!font) font = TTF_OpenFont("C:\\Windows\\Fonts\\tahoma.ttf", fontSize);
        return font;
    }

    void renderText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, int x, int y, SDL_Color color) {
        if (text.empty()) return;
        SDL_Surface* surf = TTF_RenderText_Blended(font, text.c_str(), color);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_Rect r = {x, y, surf->w, surf->h};
            SDL_RenderCopy(renderer, tex, nullptr, &r);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
    }

    void drawSchematicSymbol(SDL_Renderer* renderer, const std::string& compId, int cx, int cy) {
        SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255);
        if (compId == "RES") {
            SDL_RenderDrawLine(renderer, cx - 40, cy, cx - 20, cy);
            SDL_RenderDrawLine(renderer, cx + 20, cy, cx + 40, cy);
            SDL_Rect box = {cx - 20, cy - 8, 40, 16};
            SDL_RenderDrawRect(renderer, &box);
        }
        else if (compId == "CAP") {
            SDL_RenderDrawLine(renderer, cx - 30, cy, cx - 8, cy);
            SDL_RenderDrawLine(renderer, cx + 8, cy, cx + 30, cy);
            SDL_RenderDrawLine(renderer, cx - 8, cy - 15, cx - 8, cy + 15);
            SDL_RenderDrawLine(renderer, cx + 8, cy - 15, cx + 8, cy + 15);
        }
        else if (compId == "DC_SRC") {
            SDL_RenderDrawLine(renderer, cx - 30, cy, cx - 15, cy);
            SDL_RenderDrawLine(renderer, cx + 15, cy, cx + 30, cy);
            for (int w = 0; w < 360; w += 20) {
                float rad1 = w * 3.14159f / 180.0f;
                float rad2 = (w + 20) * 3.14159f / 180.0f;
                SDL_RenderDrawLine(renderer, cx + (int)(15 * cos(rad1)), cy + (int)(15 * sin(rad1)),
                                   cx + (int)(15 * cos(rad2)), cy + (int)(15 * sin(rad2)));
            }
            renderText(renderer, statusFont, "+", cx - 10, cy - 12, {180, 40, 40, 255});
            renderText(renderer, statusFont, "-", cx + 3, cy - 12, {180, 40, 40, 255});
        }
        else if (compId == "DIODE" || compId == "LED") {
            SDL_RenderDrawLine(renderer, cx - 30, cy, cx - 10, cy);
            SDL_RenderDrawLine(renderer, cx + 10, cy, cx + 30, cy);
            SDL_RenderDrawLine(renderer, cx - 10, cy - 12, cx - 10, cy + 12);
            SDL_RenderDrawLine(renderer, cx - 10, cy - 12, cx + 10, cy);
            SDL_RenderDrawLine(renderer, cx - 10, cy + 12, cx + 10, cy);
            SDL_RenderDrawLine(renderer, cx + 10, cy - 12, cx + 10, cy + 12);
            if (compId == "LED") {
                SDL_RenderDrawLine(renderer, cx, cy - 12, cx + 8, cy - 20);
                SDL_RenderDrawLine(renderer, cx + 5, cy - 12, cx + 13, cy - 20);
            }
        }
        else if (compId == "GND") {
            SDL_RenderDrawLine(renderer, cx, cy - 20, cx, cy);
            SDL_RenderDrawLine(renderer, cx - 15, cy, cx + 15, cy);
            SDL_RenderDrawLine(renderer, cx - 10, cy + 5, cx + 10, cy + 5);
            SDL_RenderDrawLine(renderer, cx - 5, cy + 10, cx + 5, cy + 10);
        }
        else {
            SDL_Rect box = {cx - 25, cy - 20, 50, 40};
            SDL_RenderDrawRect(renderer, &box);
            renderText(renderer, statusFont, compId, cx - 15, cy - 8, {180, 40, 40, 255});
        }
    }

public:
    AppManager(SDL_Renderer* renderer) : isRunning(true), currentState(AppState::MAIN_MENU) {
        cameraX = 0.0f; cameraY = 0.0f; zoom = 1.0f; isPanning = false;
        mouseX = 0; mouseY = 0; worldX = 0; worldY = 0;
        selectedActiveIndex = -1;
        selectedLibraryIndex = -1;
        selectedCategory = "All";
        searchQuery = "";

        mainFont = loadFont(22);
        statusFont = loadFont(15);
        smallFont = loadFont(13);

        SDL_Color white = {255, 255, 255, 255};
        btnNewProject = new Button(50, 100, 280, 50, {52, 152, 219, 255}, white, "Create New Project", mainFont, renderer);
        btnA4 = new Button(250, 150, 280, 45, {155, 89, 182, 255}, white, "A4 Size (210x297)", mainFont, renderer);
        btnA3 = new Button(250, 210, 280, 45, {155, 89, 182, 255}, white, "A3 Size (297x420)", mainFont, renderer);
        btnCustom = new Button(250, 270, 280, 45, {241, 196, 15, 255}, white, "Custom Size", mainFont, renderer);
        btnBackToMenu = new Button(5, 5, 110, 30, {231, 76, 60, 255}, white, "<- Menu", statusFont, renderer);

        btnPickDevices = new Button(125, 5, 50, 30, {46, 204, 113, 255}, white, "P", mainFont, renderer);
        btnAddSelected = new Button(520, 490, 130, 35, {46, 204, 113, 255}, white, "Add Device", statusFont, renderer);
        btnCloseDialog = new Button(660, 490, 90, 35, {231, 76, 60, 255}, white, "Close", statusFont, renderer);

        library = {
                {"RES", "10k Resistor", "Analog", "Generic 10k Ohm Resistor"},
                {"CAP", "100uF Capacitor", "Analog", "Electrolytic Capacitor 100uF"},
                {"IND", "1mH Inductor", "Analog", "Power Inductor 1mH"},
                {"DC_SRC", "DC Voltage Source", "Power Sources", "Direct Current Power Supply"},
                {"DIODE", "1N4148 Diode", "Analog", "High-speed Switching Diode"},
                {"LED", "Red LED", "Optoelectronics", "5mm Red Light Emitting Diode"},
                {"GND", "Ground (0V)", "Power Sources", "Reference Ground Node"},
                {"AND_GATE", "74HC08 AND Gate", "Digital", "Quad 2-Input AND Gate"}
        };

        recentProjects = {{"Transmission_Line_Sim", "1405/04/22"}, {"Tesla_Motor_Control", "1405/04/20"}};

        SDL_Surface* titleSurf = TTF_RenderText_Blended(mainFont, "Recent Projects:", white);
        recentProjectsTitle = SDL_CreateTextureFromSurface(renderer, titleSurf);
        titleRect = {420, 100, titleSurf->w, titleSurf->h};
        SDL_FreeSurface(titleSurf);

        int startY = 150;
        for (const auto& proj : recentProjects) {
            std::string line = "- " + proj.name + " (" + proj.lastModified + ")";
            SDL_Surface* surf = TTF_RenderText_Blended(statusFont, line.c_str(), {200, 200, 200, 255});
            recentTextTextures.push_back(SDL_CreateTextureFromSurface(renderer, surf));
            recentTextRects.push_back({420, startY, surf->w, surf->h});
            startY += 35;
            SDL_FreeSurface(surf);
        }
    }

    ~AppManager() {
        delete btnNewProject; delete btnA4; delete btnA3; delete btnCustom; delete btnBackToMenu;
        delete btnPickDevices; delete btnAddSelected; delete btnCloseDialog;
        SDL_DestroyTexture(recentProjectsTitle);
        for (auto tex : recentTextTextures) SDL_DestroyTexture(tex);
        if (mainFont) TTF_CloseFont(mainFont);
        if (statusFont) TTF_CloseFont(statusFont);
        if (smallFont) TTF_CloseFont(smallFont);
    }

    std::vector<ComponentDef> getFilteredLibrary() {
        std::vector<ComponentDef> result;
        std::string queryLower = searchQuery;
        std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(), ::tolower);

        for (const auto& comp : library) {
            bool matchesCategory = (selectedCategory == "All" || comp.category == selectedCategory);
            std::string nameLower = comp.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
            bool matchesQuery = queryLower.empty() || (nameLower.find(queryLower) != std::string::npos);

            if (matchesCategory && matchesQuery) result.push_back(comp);
        }
        return result;
    }

    SDL_FPoint getPinWorldPosition(int componentIndex, int pinIndex) const {
        if (componentIndex < 0 ||
            componentIndex >= static_cast<int>(canvasComponents.size())) {
            return {0.0f, 0.0f};
        }

        const CircuitComponent& comp = canvasComponents[componentIndex];

        if (pinIndex < 0 ||
            pinIndex >= static_cast<int>(comp.pins.size())) {
            return {comp.x, comp.y};
        }

        return {
                comp.x + comp.pins[pinIndex].relX,
                comp.y + comp.pins[pinIndex].relY
        };
    }

    void rebuildWirePath(Wire& wire) {
        SDL_FPoint start = getPinWorldPosition(wire.startComponent, wire.startPin);
        SDL_FPoint end = getPinWorldPosition(wire.endComponent, wire.endPin);

        wire.points.clear();
        wire.points.push_back(start);

        // مسیر شکسته‌ی ۹۰ درجه
        if (std::fabs(start.x - end.x) < 0.001f ||
            std::fabs(start.y - end.y) < 0.001f) {
            wire.points.push_back(end);
        } else {
            wire.points.push_back({start.x, end.y});
            wire.points.push_back(end);
        }
    }

    void rebuildAllWirePaths() {
        for (auto& wire : wires) {
            rebuildWirePath(wire);
        }
    }

    bool isPointNearSegment(float px, float py,
                            const SDL_FPoint& a,
                            const SDL_FPoint& b,
                            float tolerance) const {
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float lenSq = dx * dx + dy * dy;

        if (lenSq < 0.0001f) {
            float ddx = px - a.x;
            float ddy = py - a.y;
            return std::sqrt(ddx * ddx + ddy * ddy) <= tolerance;
        }

        float t = ((px - a.x) * dx + (py - a.y) * dy) / lenSq;
        t = std::max(0.0f, std::min(1.0f, t));

        float closestX = a.x + t * dx;
        float closestY = a.y + t * dy;

        float ddx = px - closestX;
        float ddy = py - closestY;

        return std::sqrt(ddx * ddx + ddy * ddy) <= tolerance;
    }

    int findWireAt(float wx, float wy) const {
        const float tolerance = 7.0f / std::max(zoom, 0.1f);

        for (int i = static_cast<int>(wires.size()) - 1; i >= 0; --i) {
            const Wire& wire = wires[i];

            for (size_t p = 0; p + 1 < wire.points.size(); ++p) {
                if (isPointNearSegment(
                        wx, wy, wire.points[p], wire.points[p + 1], tolerance)) {
                    return i;
                }
            }
        }

        return -1;
    }

    bool findPinAt(float wx, float wy,
                   int& componentIndex, int& pinIndex) const {
        const float sensitivityRadius = 10.0f / std::max(zoom, 0.1f);

        for (int ci = static_cast<int>(canvasComponents.size()) - 1; ci >= 0; --ci) {
            const CircuitComponent& comp = canvasComponents[ci];

            for (int pi = 0; pi < static_cast<int>(comp.pins.size()); ++pi) {
                SDL_FPoint pinPos = getPinWorldPosition(ci, pi);

                float dx = wx - pinPos.x;
                float dy = wy - pinPos.y;

                if (std::sqrt(dx * dx + dy * dy) <= sensitivityRadius) {
                    componentIndex = ci;
                    pinIndex = pi;
                    return true;
                }
            }
        }

        componentIndex = -1;
        pinIndex = -1;
        return false;
    }

    bool wireAlreadyExists(int startComponent, int startPin,
                           int endComponent, int endPin) const {
        for (const auto& wire : wires) {
            bool sameDirection =
                    wire.startComponent == startComponent &&
                    wire.startPin == startPin &&
                    wire.endComponent == endComponent &&
                    wire.endPin == endPin;

            bool reverseDirection =
                    wire.startComponent == endComponent &&
                    wire.startPin == endPin &&
                    wire.endComponent == startComponent &&
                    wire.endPin == startPin;

            if (sameDirection || reverseDirection) {
                return true;
            }
        }

        return false;
    }

    void cancelWiring() {
        isWiring = false;
        wireStartComponent = -1;
        wireStartPin = -1;
    }

    void cleanupUnusedJunctions() {
        junctions.erase(
                std::remove_if(
                        junctions.begin(),
                        junctions.end(),
                        [this](const Junction& junction) {
                            for (const auto& wire : wires) {
                                for (const auto& point : wire.points) {
                                    if (std::fabs(point.x - junction.x) < 0.01f &&
                                        std::fabs(point.y - junction.y) < 0.01f) {
                                        return false;
                                    }
                                }
                            }
                            return true;
                        }),
                junctions.end());
    }

    void removeWiresConnectedToComponent(int componentIndex) {
        wires.erase(
                std::remove_if(
                        wires.begin(),
                        wires.end(),
                        [componentIndex](const Wire& wire) {
                            return wire.startComponent == componentIndex ||
                                   wire.endComponent == componentIndex;
                        }),
                wires.end());

        for (auto& wire : wires) {
            if (wire.startComponent > componentIndex) {
                --wire.startComponent;
            }
            if (wire.endComponent > componentIndex) {
                --wire.endComponent;
            }
        }

        rebuildAllWirePaths();
        cleanupUnusedJunctions();
        hoveredWireIndex = -1;
    }

    void deleteWireAtCursor(float wx, float wy) {
        int wireIndex = findWireAt(wx, wy);

        if (wireIndex == -1) {
            return;
        }

        wires.erase(wires.begin() + wireIndex);

        cleanupUnusedJunctions();
        hoveredWireIndex = -1;

        std::cout << "Wire deleted successfully." << std::endl;
    }

    void updateHoveredElements() {
        hoveredPinComponent = -1;
        hoveredPinIndex = -1;
        hoveredWireIndex = -1;

        if (currentState != AppState::WORKSPACE ||
            mouseX <= SIDEBAR_WIDTH) {
            return;
        }

        findPinAt(worldX, worldY, hoveredPinComponent, hoveredPinIndex);

        if (hoveredPinComponent == -1) {
            hoveredWireIndex = findWireAt(worldX, worldY);
        }
    }

    void handleEvents() {
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) isRunning = false;

            // مدیریت دکمه‌های کیبورد (بخش چهارم - آیتم‌های 4.4، 5.4 و 6.4)
            if (e.type == SDL_KEYDOWN && currentState == AppState::WORKSPACE) {
                switch (e.key.keysym.sym) {
                    case SDLK_DELETE: // حذف سیم یا قطعه
                        if (isWiring) {
                            cancelWiring();
                            break;
                        }

                        // آیتم 5.5: اگر موس روی سیم باشد، همان سیم حذف می‌شود.
                        if (hoveredWireIndex != -1) {
                            deleteWireAtCursor(worldX, worldY);
                            break;
                        }

                        // حذف قطعات انتخاب‌شده و تمام سیم‌های متصل به آن‌ها
                        for (int i = static_cast<int>(canvasComponents.size()) - 1; i >= 0; --i) {
                            if (canvasComponents[i].isSelected) {
                                removeWiresConnectedToComponent(i);
                                canvasComponents.erase(canvasComponents.begin() + i);
                            }
                        }
                        rebuildAllWirePaths();
                        break;
                    case SDLK_r: // چرخش 90 درجه
                        for (auto& comp : canvasComponents) if (comp.isSelected) comp.rotate();
                        break;
                    case SDLK_m: // قرینه سازی
                        for (auto& comp : canvasComponents) if (comp.isSelected) comp.mirror(true);
                        break;
                    case SDLK_ESCAPE: // لغو جای‌گذاری قطعه یا سیم‌کشی
                        deviceToPlace = "";
                        selectedActiveIndex = -1;
                        cancelWiring();
                        for (auto& c : canvasComponents) c.isSelected = false;
                        break;
                }
            }

            if (currentState == AppState::PICK_DEVICES_DIALOG) {
                if (e.type == SDL_TEXTINPUT) {
                    searchQuery += e.text.text;
                    selectedLibraryIndex = -1;
                }
                else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_BACKSPACE && !searchQuery.empty()) {
                    searchQuery.pop_back();
                    selectedLibraryIndex = -1;
                }
            }

            if (e.type == SDL_MOUSEMOTION) {
                mouseX = e.motion.x; mouseY = e.motion.y;
                if (currentState == AppState::WORKSPACE) {
                    worldX = (mouseX - cameraX) / zoom;
                    worldY = (mouseY - cameraY) / zoom;
                    snappedWorldX = std::round(worldX / GRID_SIZE) * GRID_SIZE;
                    snappedWorldY = std::round(worldY / GRID_SIZE) * GRID_SIZE;

                    if (isPanning) {
                        cameraX = camStartX + (mouseX - panStartX);
                        cameraY = camStartY + (mouseY - panStartY);
                    }

                    // منطق نرمِ جابجایی قطعات (Snapping در هنگام Drag - آیتم 3.4)
                    if (isDragging && mouseX > SIDEBAR_WIDTH) {
                        float deltaWorldX = worldX - dragStartWorldX;
                        float deltaWorldY = worldY - dragStartWorldY;

                        for (auto& comp : canvasComponents) {
                            if (comp.isSelected) {
                                float targetX = comp.startX + deltaWorldX;
                                float targetY = comp.startY + deltaWorldY;
                                comp.x = std::round(targetX / GRID_SIZE) * GRID_SIZE;
                                comp.y = std::round(targetY / GRID_SIZE) * GRID_SIZE;
                            }
                        }

                        rebuildAllWirePaths();
                    }

                    updateHoveredElements();
                }
            }

            if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    if (currentState == AppState::MAIN_MENU) {
                        if (btnNewProject->isClicked(mouseX, mouseY)) currentState = AppState::NEW_PROJECT_DIALOG;
                    }
                    else if (currentState == AppState::NEW_PROJECT_DIALOG) {
                        if (btnA4->isClicked(mouseX, mouseY) || btnA3->isClicked(mouseX, mouseY) || btnCustom->isClicked(mouseX, mouseY)) {
                            currentState = AppState::WORKSPACE;
                        }
                    }
                    else if (currentState == AppState::WORKSPACE) {
                        if (btnBackToMenu->isClicked(mouseX, mouseY)) {
                            cancelWiring();
                            currentState = AppState::MAIN_MENU;
                        }
                        else if (btnPickDevices->isClicked(mouseX, mouseY)) {
                            currentState = AppState::PICK_DEVICES_DIALOG;
                            SDL_StartTextInput();
                        }
                            // کلیک در پنل سمت چپ -> لینک شدن به مرحله بعد
                        else if (mouseX < SIDEBAR_WIDTH && mouseY > 40 && mouseY < 570) {
                            int idx = (mouseY - 50) / 30;
                            if (idx >= 0 && idx < (int)activeDevices.size()) {
                                selectedActiveIndex = idx;
                                deviceToPlace = activeDevices[idx].id; // قطعه آماده‌ی جای‌گذاری می‌شود
                                for (auto& c : canvasComponents) c.isSelected = false; // دی‌سلکت بقیه
                            }
                        }
                            // کلیک روی بوم طراحی اصلی
                        else if (mouseX > SIDEBAR_WIDTH) {
                            bool isDoubleClick = (SDL_GetTicks() - lastClickTime < 300);
                            lastClickTime = SDL_GetTicks();

                            // 1. قرار دادن قطعه جدید روی صفحه (آیتم 1.4)
                            if (!deviceToPlace.empty()) {
                                std::string autoName = deviceToPlace.substr(0, 1) + std::to_string(componentCounter++);
                                canvasComponents.push_back(CircuitComponent(deviceToPlace, snappedWorldX, snappedWorldY, autoName));
                                deviceToPlace = ""; // ریست کردن بعد از چیدن
                                selectedActiveIndex = -1;
                            }
                            else {
                                // شروع یا پایان سیم‌کشی با کلیک روی پایه‌ها
                                if (!isDoubleClick) {
                                    int clickedPinComponent = -1;
                                    int clickedPinIndex = -1;

                                    if (findPinAt(worldX, worldY,
                                                  clickedPinComponent,
                                                  clickedPinIndex)) {

                                        if (!isWiring) {
                                            isWiring = true;
                                            wireStartComponent = clickedPinComponent;
                                            wireStartPin = clickedPinIndex;

                                            std::cout
                                                    << "Wire started from component "
                                                    << wireStartComponent
                                                    << ", pin "
                                                    << wireStartPin
                                                    << std::endl;
                                        } else {
                                            if (clickedPinComponent == wireStartComponent &&
                                                clickedPinIndex == wireStartPin) {
                                                cancelWiring();
                                            } else if (!wireAlreadyExists(
                                                    wireStartComponent,
                                                    wireStartPin,
                                                    clickedPinComponent,
                                                    clickedPinIndex)) {

                                                wires.emplace_back(
                                                        wireStartComponent,
                                                        wireStartPin,
                                                        clickedPinComponent,
                                                        clickedPinIndex);

                                                rebuildWirePath(wires.back());
                                                cancelWiring();

                                                std::cout
                                                        << "Wire created successfully."
                                                        << std::endl;
                                            } else {
                                                std::cout
                                                        << "This wire already exists."
                                                        << std::endl;
                                                cancelWiring();
                                            }
                                        }

                                        break;
                                    }

                                    // اگر روی سیم کلیک شود، Delete آن را حذف خواهد کرد.
                                    if (hoveredWireIndex != -1) {
                                        std::cout
                                                << "Wire selected. Press Delete to remove it."
                                                << std::endl;
                                        break;
                                    }
                                }

                                // بررسی کلیک روی قطعات موجود
                                bool clickedOnComponent = false;
                                for (auto it = canvasComponents.rbegin(); it != canvasComponents.rend(); ++it) {
                                    if (it->contains(worldX, worldY)) {
                                        clickedOnComponent = true;

                                        // پنجره ویژگی‌ها با دابل کلیک (آیتم 7.4)
                                        if (isDoubleClick) {
                                            propertiesTarget = &(*it);
                                            std::cout << "Opened Properties for: " << it->name << std::endl;
                                            break;
                                        }

                                        const Uint8* state = SDL_GetKeyboardState(NULL);
                                        if (!state[SDL_SCANCODE_LSHIFT] && !it->isSelected) {
                                            for (auto& c : canvasComponents) c.isSelected = false;
                                        }
                                        it->isSelected = true;

                                        // شروع Drag
                                        isDragging = true;
                                        dragStartWorldX = worldX;
                                        dragStartWorldY = worldY;
                                        for (auto& c : canvasComponents) {
                                            if (c.isSelected) {
                                                c.startX = c.x;
                                                c.startY = c.y;
                                            }
                                        }
                                        break;
                                    }
                                }

                                // 3. شروع انتخاب چندگانه با کشیدن موس (آیتم 2.2.4)
                                if (!clickedOnComponent) {
                                    isSelecting = true;
                                    selectStartX = worldX;
                                    selectStartY = worldY;

                                    const Uint8* state = SDL_GetKeyboardState(NULL);
                                    if (!state[SDL_SCANCODE_LSHIFT]) {
                                        for (auto& c : canvasComponents) c.isSelected = false;
                                    }
                                }
                            }
                        }
                    }
                    else if (currentState == AppState::PICK_DEVICES_DIALOG) {
                        if (btnCloseDialog->isClicked(mouseX, mouseY)) {
                            currentState = AppState::WORKSPACE;
                            SDL_StopTextInput();
                        }
                        if (mouseX >= 110 && mouseX <= 230) {
                            std::vector<std::string> cats = {"All", "Analog", "Digital", "Power Sources", "Optoelectronics"};
                            int startY = 160;
                            for (size_t i = 0; i < cats.size(); ++i) {
                                if (mouseY >= startY + (int)i * 30 && mouseY <= startY + (int)i * 30 + 25) {
                                    selectedCategory = cats[i];
                                    selectedLibraryIndex = -1;
                                }
                            }
                        }
                        auto filtered = getFilteredLibrary();
                        if (mouseX >= 250 && mouseX <= 500 && mouseY >= 160 && mouseY <= 470) {
                            int idx = (mouseY - 160) / 30;
                            if (idx >= 0 && idx < (int)filtered.size()) selectedLibraryIndex = idx;
                        }
                        if (btnAddSelected->isClicked(mouseX, mouseY) && selectedLibraryIndex != -1 && selectedLibraryIndex < (int)filtered.size()) {
                            activeDevices.push_back(filtered[selectedLibraryIndex]);
                            currentState = AppState::WORKSPACE;
                            SDL_StopTextInput();
                        }
                    }
                }
                else if (e.button.button == SDL_BUTTON_RIGHT) {
                    if (currentState == AppState::WORKSPACE) {
                        if (mouseX > SIDEBAR_WIDTH) {
                            isPanning = true; panStartX = mouseX; panStartY = mouseY;
                            camStartX = cameraX; camStartY = cameraY;
                        } else {
                            int idx = (mouseY - 50) / 30;
                            if (idx >= 0 && idx < (int)activeDevices.size()) {
                                activeDevices.erase(activeDevices.begin() + idx);
                                if (selectedActiveIndex >= (int)activeDevices.size()) selectedActiveIndex = -1;
                                deviceToPlace = ""; // ریست کردن جای‌گذاری در صورت حذف از لیست
                            }
                        }
                    }
                }
            }

            if (e.type == SDL_MOUSEBUTTONUP) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    isDragging = false;

                    // پایان انتخاب چندگانه و بررسی قطعات داخل کادر
                    if (isSelecting) {
                        isSelecting = false;
                        float minX = std::min(selectStartX, worldX);
                        float maxX = std::max(selectStartX, worldX);
                        float minY = std::min(selectStartY, worldY);
                        float maxY = std::max(selectStartY, worldY);

                        for (auto& comp : canvasComponents) {
                            if (comp.x >= minX && comp.x <= maxX && comp.y >= minY && comp.y <= maxY) {
                                comp.isSelected = true;
                            }
                        }
                    }
                }
                else if (e.button.button == SDL_BUTTON_RIGHT) isPanning = false;
            }

            if (e.type == SDL_MOUSEWHEEL && currentState == AppState::WORKSPACE && mouseX > SIDEBAR_WIDTH) {
                float oldZoom = zoom;
                if (e.wheel.y > 0) zoom *= 1.1f;
                else if (e.wheel.y < 0) zoom /= 1.1f;
                if (zoom < 0.1f) zoom = 0.1f;
                if (zoom > 5.0f) zoom = 5.0f;
                cameraX = mouseX - (mouseX - cameraX) * (zoom / oldZoom);
                cameraY = mouseY - (mouseY - cameraY) * (zoom / oldZoom);
                worldX = (mouseX - cameraX) / zoom;
                worldY = (mouseY - cameraY) / zoom;
                snappedWorldX = std::round(worldX / GRID_SIZE) * GRID_SIZE;
                snappedWorldY = std::round(worldY / GRID_SIZE) * GRID_SIZE;
            }
        }
    }

    void render(SDL_Renderer* renderer) {
        if (currentState == AppState::WORKSPACE) {
            rebuildAllWirePaths();
            updateHoveredElements();
        }

        if (currentState == AppState::MAIN_MENU) {
            SDL_SetRenderDrawColor(renderer, 40, 44, 52, 255);
            SDL_RenderClear(renderer);
            btnNewProject->draw(renderer);
            SDL_RenderCopy(renderer, recentProjectsTitle, nullptr, &titleRect);
            for (size_t i = 0; i < recentTextTextures.size(); ++i) SDL_RenderCopy(renderer, recentTextTextures[i], nullptr, &recentTextRects[i]);
        }
        else if (currentState == AppState::NEW_PROJECT_DIALOG) {
            SDL_SetRenderDrawColor(renderer, 40, 44, 52, 255);
            SDL_RenderClear(renderer);
            btnA4->draw(renderer); btnA3->draw(renderer); btnCustom->draw(renderer);
        }
        else if (currentState == AppState::WORKSPACE || currentState == AppState::PICK_DEVICES_DIALOG) {
            SDL_SetRenderDrawColor(renderer, 245, 247, 250, 255);
            SDL_RenderClear(renderer);

            SDL_SetRenderDrawColor(renderer, 180, 185, 195, 255);
            float scaledGridSize = GRID_SIZE * zoom;
            float offsetX = std::fmod(cameraX, scaledGridSize);
            float offsetY = std::fmod(cameraY, scaledGridSize);
            if (offsetX < 0) offsetX += scaledGridSize;
            if (offsetY < 0) offsetY += scaledGridSize;

            for (float x = std::max((float)SIDEBAR_WIDTH, offsetX); x < 900; x += scaledGridSize)
                for (float y = offsetY; y < 600 - 30; y += scaledGridSize)
                    SDL_RenderDrawPoint(renderer, (int)x, (int)y);

            // ===============================================
            // --- رسم سیم‌ها (بخش پنجم) ---
            // ===============================================
            rebuildAllWirePaths();

            for (size_t wireIndex = 0; wireIndex < wires.size(); ++wireIndex) {
                const Wire& wire = wires[wireIndex];

                if (wire.points.size() < 2) {
                    continue;
                }

                if (static_cast<int>(wireIndex) == hoveredWireIndex) {
                    SDL_SetRenderDrawColor(renderer, 230, 126, 34, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer, 70, 70, 70, 255);
                }

                for (size_t p = 0; p + 1 < wire.points.size(); ++p) {
                    int x1 = static_cast<int>(
                            wire.points[p].x * zoom + cameraX);
                    int y1 = static_cast<int>(
                            wire.points[p].y * zoom + cameraY);

                    int x2 = static_cast<int>(
                            wire.points[p + 1].x * zoom + cameraX);
                    int y2 = static_cast<int>(
                            wire.points[p + 1].y * zoom + cameraY);

                    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
                    SDL_RenderDrawLine(renderer, x1, y1 + 1, x2, y2 + 1);
                }
            }

            // ===============================================
            // --- رسم قطعات چیده شده روی بوم (بخش چهارم) ---
            // ===============================================
            for (size_t compIndex = 0; compIndex < canvasComponents.size(); ++compIndex) {
                const auto& comp = canvasComponents[compIndex];
                float currentW = (comp.rotation % 180 == 0) ? comp.width : comp.height;
                float currentH = (comp.rotation % 180 == 0) ? comp.height : comp.width;

                int screenX = (int)(comp.x * zoom + cameraX);
                int screenY = (int)(comp.y * zoom + cameraY);
                int screenW = (int)(currentW * zoom);
                int screenH = (int)(currentH * zoom);

                SDL_Rect compRect = { screenX - screenW / 2, screenY - screenH / 2, screenW, screenH };

                // رسم بدنه قطعه
                SDL_SetRenderDrawColor(renderer, 230, 235, 240, 255);
                SDL_RenderFillRect(renderer, &compRect);

                // تغییر رنگ در صورت انتخاب (آیتم 2.2.4)
                if (comp.isSelected) {
                    SDL_SetRenderDrawColor(renderer, 231, 76, 60, 255); // قرمز هایلایت
                    SDL_RenderDrawRect(renderer, &compRect);
                    SDL_Rect innerRect = { compRect.x+1, compRect.y+1, compRect.w-2, compRect.h-2 };
                    SDL_RenderDrawRect(renderer, &innerRect); // حاشیه ضخیم تر
                } else {
                    SDL_SetRenderDrawColor(renderer, 52, 152, 219, 255); // رنگ عادی
                    SDL_RenderDrawRect(renderer, &compRect);
                }

                // چاپ اسم قطعه (برای درک چرخش و مشخصات)
                renderText(renderer, statusFont, comp.name, screenX - 10, screenY - 20, {20, 20, 20, 255});
                renderText(renderer, smallFont, comp.value, screenX - 10, screenY + 5, {100, 100, 100, 255});

                // رسم دقیق پایه‌ها با در نظر گرفتن چرخش، زوم و Highlight
                for (size_t pinIndex = 0; pinIndex < comp.pins.size(); ++pinIndex) {
                    const auto& pin = comp.pins[pinIndex];

                    int pinScreenX = static_cast<int>(
                            (comp.x + pin.relX) * zoom + cameraX);
                    int pinScreenY = static_cast<int>(
                            (comp.y + pin.relY) * zoom + cameraY);

                    if (static_cast<int>(compIndex) == hoveredPinComponent &&
                        static_cast<int>(pinIndex) == hoveredPinIndex) {
                        SDL_SetRenderDrawColor(renderer, 241, 196, 15, 255);
                        SDL_Rect pinHighlight = {
                                pinScreenX - 5, pinScreenY - 5, 10, 10
                        };
                        SDL_RenderFillRect(renderer, &pinHighlight);
                    } else {
                        SDL_SetRenderDrawColor(renderer, 46, 204, 113, 255);
                        SDL_Rect pinRect = {
                                pinScreenX - 3, pinScreenY - 3, 6, 6
                        };
                        SDL_RenderFillRect(renderer, &pinRect);
                    }

                    // رسم خط اتصال پایه به بدنه
                    SDL_RenderDrawLine(
                            renderer,
                            screenX, screenY,
                            pinScreenX, pinScreenY);
                }
            }

            // سیم موقت هنگام اتصال پایه اول به موس
            if (isWiring &&
                wireStartComponent >= 0 &&
                wireStartPin >= 0 &&
                mouseX > SIDEBAR_WIDTH) {

                SDL_FPoint start =
                        getPinWorldPosition(wireStartComponent, wireStartPin);

                SDL_FPoint end = {snappedWorldX, snappedWorldY};
                SDL_FPoint corner = {start.x, end.y};

                int sx = static_cast<int>(start.x * zoom + cameraX);
                int sy = static_cast<int>(start.y * zoom + cameraY);

                int cx = static_cast<int>(corner.x * zoom + cameraX);
                int cy = static_cast<int>(corner.y * zoom + cameraY);

                int ex = static_cast<int>(end.x * zoom + cameraX);
                int ey = static_cast<int>(end.y * zoom + cameraY);

                SDL_SetRenderDrawColor(renderer, 52, 152, 219, 255);
                SDL_RenderDrawLine(renderer, sx, sy, cx, cy);
                SDL_RenderDrawLine(renderer, cx, cy, ex, ey);
            }

            // رسم کادر انتخاب چندگانه (Multi-select Box)
            if (isSelecting && mouseX > SIDEBAR_WIDTH) {
                float startScreenX = selectStartX * zoom + cameraX;
                float startScreenY = selectStartY * zoom + cameraY;
                float endScreenX = worldX * zoom + cameraX;
                float endScreenY = worldY * zoom + cameraY;

                SDL_Rect selRect = {
                        (int)std::min(startScreenX, endScreenX), (int)std::min(startScreenY, endScreenY),
                        (int)std::abs(endScreenX - startScreenX), (int)std::abs(endScreenY - startScreenY)
                };

                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 52, 152, 219, 80);
                SDL_RenderFillRect(renderer, &selRect);
                SDL_SetRenderDrawColor(renderer, 41, 128, 185, 255);
                SDL_RenderDrawRect(renderer, &selRect);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }

            // رسم شبح (Ghost) قطعه در زمان جای‌گذاری
            if (!deviceToPlace.empty() && mouseX > SIDEBAR_WIDTH) {
                int ghostX = (int)(snappedWorldX * zoom + cameraX);
                int ghostY = (int)(snappedWorldY * zoom + cameraY);
                SDL_Rect ghostR = { ghostX - 40, ghostY - 20, 80, 40 }; // سایز تقریبی پیش‌فرض
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 46, 204, 113, 100); // سبز نیمه‌شفاف
                SDL_RenderFillRect(renderer, &ghostR);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                renderText(renderer, statusFont, deviceToPlace, ghostX - 20, ghostY - 10, {30, 150, 30, 255});
            }

            // کرسر قرمز رنگ Snap to Grid
            if (mouseX > SIDEBAR_WIDTH) {
                int screenSnapX = (int)(snappedWorldX * zoom + cameraX);
                int screenSnapY = (int)(snappedWorldY * zoom + cameraY);
                SDL_Rect snapCursor = {screenSnapX - 3, screenSnapY - 3, 6, 6};
                SDL_SetRenderDrawColor(renderer, 231, 76, 60, 255);
                SDL_RenderDrawRect(renderer, &snapCursor);
            }
            // ===============================================

            SDL_Rect sidebar = {0, 0, SIDEBAR_WIDTH, 600 - 30};
            SDL_SetRenderDrawColor(renderer, 220, 225, 230, 255);
            SDL_RenderFillRect(renderer, &sidebar);
            SDL_SetRenderDrawColor(renderer, 160, 165, 170, 255);
            SDL_RenderDrawLine(renderer, SIDEBAR_WIDTH, 0, SIDEBAR_WIDTH, 600 - 30);

            btnBackToMenu->draw(renderer);
            btnPickDevices->draw(renderer);

            renderText(renderer, statusFont, "DEVICES", 10, 45, {80, 80, 80, 255});
            SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
            SDL_RenderDrawLine(renderer, 10, 65, SIDEBAR_WIDTH - 10, 65);

            int devY = 70;
            for (size_t i = 0; i < activeDevices.size(); ++i) {
                SDL_Rect itemRect = {5, devY, SIDEBAR_WIDTH - 10, 26};
                if ((int)i == selectedActiveIndex) {
                    SDL_SetRenderDrawColor(renderer, 52, 152, 219, 255);
                    SDL_RenderFillRect(renderer, &itemRect);
                    renderText(renderer, statusFont, activeDevices[i].id, 10, devY + 4, {255, 255, 255, 255});
                } else {
                    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
                    SDL_RenderFillRect(renderer, &itemRect);
                    renderText(renderer, statusFont, activeDevices[i].id, 10, devY + 4, {40, 40, 40, 255});
                }
                devY += 30;
            }

            SDL_Rect statusBar = {0, 600 - 30, 900, 30};
            SDL_SetRenderDrawColor(renderer, 200, 205, 215, 255);
            SDL_RenderFillRect(renderer, &statusBar);
            SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
            SDL_RenderDrawLine(renderer, 0, 600 - 30, 900, 600 - 30);

            std::string modeText;

            if (isWiring) {
                modeText = "(Wire mode - Click another pin / ESC to cancel)";
            } else if (!deviceToPlace.empty()) {
                modeText = "(Place Mode - Press ESC to cancel)";
            } else if (hoveredWireIndex != -1) {
                modeText = "(Wire selected - Press Delete)";
            } else {
                modeText = "(Select/Drag mode)";
            }

            std::string statusText =
                    "Zoom: " + std::to_string((int)(zoom * 100)) +
                    "%  |  X: " + std::to_string((int)snappedWorldX) +
                    " Y: " + std::to_string((int)snappedWorldY) +
                    "  |  Wires: " + std::to_string(wires.size()) +
                    "  |  " + modeText;
            renderText(renderer, statusFont, statusText, 15, 600 - 23, {50, 50, 50, 255});

            if (currentState == AppState::PICK_DEVICES_DIALOG) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 120);
                SDL_Rect fullScreen = {0, 0, 900, 600};
                SDL_RenderFillRect(renderer, &fullScreen);

                SDL_Rect dialog = {100, 50, 700, 490};
                SDL_SetRenderDrawColor(renderer, 240, 242, 245, 255);
                SDL_RenderFillRect(renderer, &dialog);
                SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
                SDL_RenderDrawRect(renderer, &dialog);

                renderText(renderer, mainFont, "Pick Devices - Component Library", 120, 65, {30, 30, 30, 255});

                renderText(renderer, statusFont, "Keywords / Search:", 120, 105, {50, 50, 50, 255});
                SDL_Rect searchBox = {250, 100, 430, 28};
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderFillRect(renderer, &searchBox);
                SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
                SDL_RenderDrawRect(renderer, &searchBox);
                renderText(renderer, statusFont, searchQuery + "|", 255, 105, {20, 20, 20, 255});

                renderText(renderer, statusFont, "Category:", 120, 140, {50, 50, 50, 255});
                std::vector<std::string> categories = {"All", "Analog", "Digital", "Power Sources", "Optoelectronics"};
                int catY = 160;
                for (const auto& cat : categories) {
                    SDL_Rect catRect = {110, catY, 120, 25};
                    if (cat == selectedCategory) {
                        SDL_SetRenderDrawColor(renderer, 52, 152, 219, 255);
                        SDL_RenderFillRect(renderer, &catRect);
                        renderText(renderer, smallFont, cat, 115, catY + 5, {255, 255, 255, 255});
                    } else {
                        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
                        SDL_RenderFillRect(renderer, &catRect);
                        renderText(renderer, smallFont, cat, 115, catY + 5, {30, 30, 30, 255});
                    }
                    catY += 30;
                }

                SDL_Rect tableBox = {250, 140, 250, 330};
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderFillRect(renderer, &tableBox);
                SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
                SDL_RenderDrawRect(renderer, &tableBox);

                auto filtered = getFilteredLibrary();
                if (filtered.empty()) {
                    renderText(renderer, statusFont, "No components found!", 280, 180, {200, 50, 50, 255});
                } else {
                    int listY = 145;
                    for (size_t i = 0; i < filtered.size(); ++i) {
                        SDL_Rect itemR = {252, listY, 246, 26};
                        if ((int)i == selectedLibraryIndex) {
                            SDL_SetRenderDrawColor(renderer, 41, 128, 185, 255);
                            SDL_RenderFillRect(renderer, &itemR);
                            renderText(renderer, statusFont, filtered[i].name, 258, listY + 4, {255, 255, 255, 255});
                        } else {
                            renderText(renderer, statusFont, filtered[i].name, 258, listY + 4, {30, 30, 30, 255});
                        }
                        listY += 28;
                    }
                }

                SDL_Rect previewBox = {515, 140, 265, 200};
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderFillRect(renderer, &previewBox);
                SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
                SDL_RenderDrawRect(renderer, &previewBox);
                renderText(renderer, smallFont, "Schematic Preview:", 520, 145, {120, 120, 120, 255});

                SDL_Rect descBox = {515, 350, 265, 120};
                SDL_SetRenderDrawColor(renderer, 250, 250, 250, 255);
                SDL_RenderFillRect(renderer, &descBox);
                SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
                SDL_RenderDrawRect(renderer, &descBox);

                if (selectedLibraryIndex != -1 && selectedLibraryIndex < (int)filtered.size()) {
                    drawSchematicSymbol(renderer, filtered[selectedLibraryIndex].id, 515 + 132, 140 + 100);
                    renderText(renderer, statusFont, "Device: " + filtered[selectedLibraryIndex].id, 522, 358, {20, 20, 20, 255});
                    renderText(renderer, smallFont, "Category: " + filtered[selectedLibraryIndex].category, 522, 382, {80, 80, 80, 255});
                    renderText(renderer, smallFont, filtered[selectedLibraryIndex].description, 522, 406, {100, 100, 100, 255});
                }

                btnAddSelected->draw(renderer);
                btnCloseDialog->draw(renderer);
            }
        }
        SDL_RenderPresent(renderer);
    }

    bool getIsRunning() const { return isRunning; }
};

#undef main
int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window* window = SDL_CreateWindow("Proteus Simulator - Part 5.5",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          900, 600, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    AppManager app(renderer);

    while (app.getIsRunning()) {
        app.handleEvents();
        app.render(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
