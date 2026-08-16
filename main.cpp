//////////  بخش 01 - ساختارهای اولیه و حالت برنامه
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <limits>
#include <fstream>
#include <ctime>

enum class AppState {
    MAIN_MENU,
    NEW_PROJECT_DIALOG,
    WORKSPACE,
    PICK_DEVICES_DIALOG
};

struct ComponentDef {
    std::string id;
    std::string name;
    std::string category;
    std::string description;
};

struct RecentProject {
    std::string name;
    std::string lastModified;
};

enum class LogicState {
    LOW,
    HIGH,
    UNDEFINED
};

static const float LOGIC_LOW_VOLTAGE = 0.0f;
static const float LOGIC_HIGH_VOLTAGE = 5.0f;
static const float LOGIC_UNDEFINED_MIN = 0.8f;
static const float LOGIC_UNDEFINED_MAX = 2.0f;

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

struct Pin {
    float relX, relY;
};

class CircuitComponent {
public:
    std::string type;
    std::string name;
    std::string value;
    float x, y;
    float startX, startY;
    float width, height;
    int rotation;
    bool isMirroredX;
    bool isMirroredY;
    bool isSelected;
    std::vector<Pin> pins;

    std::vector<LogicState> pinLogic;
    LogicState storedLogicState = LogicState::LOW;
    LogicState previousClockState = LogicState::LOW;
    bool switchOn = false;
    bool buttonPressed = false;
    bool outputHigh = false;
    bool hasUndefinedWarning = false;
    Uint32 lastClockTick = 0;
    Uint32 propagationDelayMs = 0;

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
//////////  بخش 02 - Wire و Junction و Pin

        if (type == "RES") {
            pins = {{-40, 0}, {40, 0}};
            value = "10k";
            propagationDelayMs = 0;
        } else if (type == "CAP") {
            pins = {{-40, 0}, {40, 0}};
            value = "1uF";
        } else if (type == "IND") {
            pins = {{-40, 0}, {40, 0}};
            value = "1mH";
        } else if (type == "DIODE" || type == "LED") {
            pins = {{-40, 0}, {40, 0}};
            value = (type == "LED") ? "Red" : "1N4148";
        } else if (type == "GND") {
            pins = {{0, -30}};
            width = 60; height = 60;
            value = "0V";
        } else if (type == "DC_SRC") {
            pins = {{0, -30}, {0, 30}};
            width = 60; height = 60;
            value = "5V";
            outputHigh = true;
        } else if (type == "BATTERY") {
            pins = {{0, -30}, {0, 30}};
            width = 60; height = 60;
            value = "9V";
            outputHigh = true;
        } else if (type == "CLOCK") {
            pins = {{30, 0}};
            width = 60; height = 50;
            value = "1Hz";
            lastClockTick = SDL_GetTicks();
        } else if (type == "SWITCH" || type == "BUTTON") {
            pins = {{-35, 0}, {35, 0}};
            width = 80; height = 40;
            value = (type == "SWITCH") ? "OFF" : "PUSH";
        } else if (type == "AND_GATE" || type == "OR_GATE" ||
                   type == "XOR_GATE" || type == "NAND_GATE") {
            pins = {{-35, -12}, {-35, 12}, {35, 0}};
            width = 75; height = 55;
            value = "2-IN";
            propagationDelayMs = 10;
        } else if (type == "NOT_GATE") {
            pins = {{-35, 0}, {35, 0}};
            width = 75; height = 45;
            value = "NOT";
            propagationDelayMs = 10;
        } else if (type == "DFF") {
            pins = {{-35, -12}, {-35, 12}, {35, 0}};
            width = 75; height = 60;
            value = "D-FF";
            propagationDelayMs = 10;
        } else if (type == "SEG7") {
            pins = {{-40,-30},{-40,-10},{-40,10},{-40,30},
                    {40,-30},{40,-10},{40,10}};
            width = 90; height = 100;
            value = "7-SEG";
        } else {
            pins = {{-30, -10}, {-30, 10}, {30, 0}};
            width = 60; height = 60;
            value = "";
        }

        pinLogic.assign(pins.size(), LogicState::UNDEFINED);
    }

    bool isInputPin(int pinIndex) const {
        if (type == "DFF") return pinIndex == 0 || pinIndex == 1;
        if (type == "NOT_GATE") return pinIndex == 0;
        if (type == "AND_GATE" || type == "OR_GATE" ||
            type == "XOR_GATE" || type == "NAND_GATE") {
            return pinIndex == 0 || pinIndex == 1;
        }
        if (type == "LED") return pinIndex == 0;
        if (type == "SEG7") return pinIndex >= 0 && pinIndex < 7;
        if (type == "SWITCH" || type == "BUTTON") return pinIndex == 0;
        if (type == "RES" || type == "CAP" || type == "IND" || type == "DIODE") return pinIndex == 0;
        return false;
    }

    bool isOutputPin(int pinIndex) const {
        if (type == "DFF") return pinIndex == 2;
        if (type == "NOT_GATE" || type == "AND_GATE" || type == "OR_GATE" ||
            type == "XOR_GATE" || type == "NAND_GATE") {
            return (type == "NOT_GATE") ? pinIndex == 1 : pinIndex == 2;
        }
        if (type == "DC_SRC" || type == "BATTERY" || type == "GND" ||
            type == "CLOCK") {
            return pinIndex == 0;
        }
        if (type == "SWITCH" || type == "BUTTON") {
            return pinIndex == 1;
        }
        return false;
    }
//////////  بخش 03 - ساختار Component و سازنده

    LogicState outputForPin(int pinIndex) const {
        if (!isOutputPin(pinIndex)) return LogicState::UNDEFINED;

        if (type == "GND") return LogicState::LOW;
        if (type == "DC_SRC" || type == "BATTERY") return LogicState::HIGH;
        if (type == "CLOCK") return outputHigh ? LogicState::HIGH : LogicState::LOW;

        if (type == "SWITCH") {
            if (!switchOn) return LogicState::LOW;
            return pinLogic.empty() ? LogicState::UNDEFINED : pinLogic[0];
        }
        if (type == "BUTTON") {
            if (!buttonPressed) return LogicState::LOW;
            return pinLogic.empty() ? LogicState::UNDEFINED : pinLogic[0];
        }
        if (type == "DFF") return storedLogicState;

        if (pinIndex >= 0 && pinIndex < static_cast<int>(pinLogic.size()))
            return pinLogic[pinIndex];
        return LogicState::UNDEFINED;
    }

    void setInput(int pinIndex, LogicState state) {
        if (pinIndex >= 0 && pinIndex < static_cast<int>(pinLogic.size())) {
            pinLogic[pinIndex] = state;
        }
    }

    bool contains(float wx, float wy) const {
        float currentW = (rotation % 180 == 0) ? width : height;
        float currentH = (rotation % 180 == 0) ? height : width;
        return (wx >= x - currentW / 2 && wx <= x + currentW / 2 &&
                wy >= y - currentH / 2 && wy <= y + currentH / 2);
    }

    void rotate() {
        rotation = (rotation + 90) % 360;
        for (auto& pin : pins) {
            float oldX = pin.relX;
            pin.relX = -pin.relY;
            pin.relY = oldX;
        }
    }

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
//////////  بخش 04 - منطق پایه‌ها و رفتار قطعات

enum class SimulationMode {
    STOPPED,
    RUNNING,
    PAUSED
};

struct CircuitSnapshot {
    std::vector<CircuitComponent> components;
    std::vector<Wire> wires;
    std::vector<Junction> junctions;
};

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
    Button* btnRun;
    Button* btnPause;
    Button* btnStop;
    Button* btnAddSelected;
    Button* btnCloseDialog;

    SDL_Texture* recentProjectsTitle;
    SDL_Rect titleRect;
    std::vector<SDL_Texture*> recentTextTextures;
    std::vector<SDL_Rect> recentTextRects;
    SDL_Renderer* rendererRef;
    std::string projectNameBuffer;
    std::string currentProjectName;
    bool projectNameActive = false;

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

    std::vector<CircuitComponent> canvasComponents;
    std::string deviceToPlace = "";
    int componentCounter = 1;

    bool isDragging = false;
    float dragStartWorldX = 0, dragStartWorldY = 0;

    bool isSelecting = false;
    float selectStartX = 0, selectStartY = 0;

    std::vector<Wire> wires;
    std::vector<Junction> junctions;

    bool isWiring = false;
    int wireStartComponent = -1;
    int wireStartPin = -1;

    int hoveredWireIndex = -1;
    int hoveredPinComponent = -1;
    int hoveredPinIndex = -1;

    int propertiesTargetIndex = -1;
    bool showProperties = false;
    int propertiesField = 0;
    std::string propertyLabelBuffer;
    std::string propertyValueBuffer;

    SimulationMode simulationMode = SimulationMode::STOPPED;
    bool stepRequested = false;
    float wireAnimationOffset = 0.0f;

    std::vector<std::string> simulationLog;
    std::vector<CircuitSnapshot> undoStack;
    const size_t MAX_UNDO = 1;

    CircuitComponent* propertiesTarget = nullptr;
    Uint32 lastClickTime = 0;

//////////  بخش 05 - کلاس Button
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
        else if (compId == "BATTERY") {
            SDL_RenderDrawLine(renderer, cx - 30, cy, cx - 10, cy);
            SDL_RenderDrawLine(renderer, cx + 10, cy, cx + 30, cy);
            SDL_RenderDrawLine(renderer, cx - 5, cy - 12, cx - 5, cy + 12);
            SDL_RenderDrawLine(renderer, cx + 5, cy - 7, cx + 5, cy + 7);
        }
        else if (compId == "CLOCK") {
            SDL_Rect box = {cx - 20, cy - 15, 40, 30};
            SDL_RenderDrawRect(renderer, &box);
            SDL_RenderDrawLine(renderer, cx - 16, cy + 5, cx - 8, cy + 5);
            SDL_RenderDrawLine(renderer, cx - 8, cy + 5, cx - 8, cy - 5);
            SDL_RenderDrawLine(renderer, cx - 8, cy - 5, cx, cy - 5);
            SDL_RenderDrawLine(renderer, cx, cy - 5, cx, cy + 5);
            SDL_RenderDrawLine(renderer, cx, cy + 5, cx + 8, cy + 5);
        }
        else if (compId == "SWITCH" || compId == "BUTTON") {
            SDL_RenderDrawLine(renderer, cx - 30, cy, cx - 10, cy);
            SDL_RenderDrawLine(renderer, cx + 10, cy, cx + 30, cy);
            SDL_RenderDrawLine(renderer, cx - 8, cy + 5, cx + 8, cy - 10);
            renderText(renderer, smallFont, compId == "SWITCH" ? "SW" : "PB",
                       cx - 12, cy - 22, {180, 40, 40, 255});
        }
        else if (compId == "AND_GATE" || compId == "NAND_GATE" ||
                 compId == "OR_GATE" || compId == "XOR_GATE" || compId == "NOT_GATE") {
            SDL_Rect box = {cx - 28, cy - 20, 56, 40};
            SDL_RenderDrawRect(renderer, &box);
            std::string label = (compId == "AND_GATE" ? "AND" :
                                 compId == "NAND_GATE" ? "NAND" :
                                 compId == "OR_GATE" ? "OR" :
                                 compId == "XOR_GATE" ? "XOR" : "NOT");
            renderText(renderer, smallFont, label, cx - 20, cy - 6, {180, 40, 40, 255});
        }
        else if (compId == "DFF") {
            SDL_Rect box = {cx - 28, cy - 22, 56, 44};
            SDL_RenderDrawRect(renderer, &box);
            renderText(renderer, smallFont, "D", cx - 20, cy - 6, {180, 40, 40, 255});
            renderText(renderer, smallFont, "Q", cx + 8, cy - 6, {180, 40, 40, 255});
        }
        else if (compId == "SEG7") {
            SDL_Rect box = {cx - 22, cy - 32, 44, 64};
            SDL_RenderDrawRect(renderer, &box);
            SDL_RenderDrawLine(renderer, cx - 8, cy - 22, cx + 8, cy - 22);
            SDL_RenderDrawLine(renderer, cx - 8, cy, cx + 8, cy);
            SDL_RenderDrawLine(renderer, cx - 8, cy + 22, cx + 8, cy + 22);
        }
        else {
            SDL_Rect box = {cx - 25, cy - 20, 50, 40};
            SDL_RenderDrawRect(renderer, &box);
            renderText(renderer, statusFont, compId, cx - 15, cy - 8, {180, 40, 40, 255});
        }
    }
//////////  بخش 06 - وضعیت Simulation و Snapshot

    std::string currentDateText() const {
        std::time_t now = std::time(nullptr);
        std::tm localTime{};
        if (const std::tm* tempTime = std::localtime(&now)) {
            localTime = *tempTime;
        }
        char buffer[32]{};
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &localTime);
        return std::string(buffer);
    }

    void loadRecentProjects() {
        recentProjects.clear();
        std::ifstream file("recent_projects.txt");
        if (file) {
            std::string line;
            while (std::getline(file, line) && recentProjects.size() < 8) {
                size_t sep = line.find('|');
                if (sep == std::string::npos) continue;
                std::string name = line.substr(0, sep);
                std::string date = line.substr(sep + 1);
                if (!name.empty()) recentProjects.push_back({name, date});
            }
        }
        if (recentProjects.empty()) {
            recentProjects = {{"Transmission_Line_Sim", "1405/04/22"}, {"Tesla_Motor_Control", "1405/04/20"}};
        }
    }

    void saveRecentProjects() {
        std::ofstream file("recent_projects.txt", std::ios::trunc);
        if (!file) return;
        for (const auto& project : recentProjects) {
            file << project.name << "|" << project.lastModified << "\n";
        }
    }

    void refreshRecentProjectTextures() {
        if (!rendererRef || !mainFont || !statusFont) return;
        if (recentProjectsTitle) {
            SDL_DestroyTexture(recentProjectsTitle);
            recentProjectsTitle = nullptr;
        }
        for (auto tex : recentTextTextures) {
            if (tex) SDL_DestroyTexture(tex);
        }
        recentTextTextures.clear();
        recentTextRects.clear();

        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface* titleSurf = TTF_RenderText_Blended(mainFont, "Recent Projects:", white);
        if (titleSurf) {
            recentProjectsTitle = SDL_CreateTextureFromSurface(rendererRef, titleSurf);
            titleRect = {420, 100, titleSurf->w, titleSurf->h};
            SDL_FreeSurface(titleSurf);
        }

        int startY = 150;
        for (const auto& proj : recentProjects) {
            std::string line = "- " + proj.name + " (" + proj.lastModified + ")";
            SDL_Surface* surf = TTF_RenderText_Blended(statusFont, line.c_str(), {200, 200, 200, 255});
            if (surf) {
                recentTextTextures.push_back(SDL_CreateTextureFromSurface(rendererRef, surf));
                recentTextRects.push_back({420, startY, surf->w, surf->h});
                startY += 35;
                SDL_FreeSurface(surf);
            }
        }
    }

    void addRecentProject(const std::string& name) {
        std::string cleanName = name.empty() ? "Untitled_Project" : name;
        recentProjects.erase(
                std::remove_if(recentProjects.begin(), recentProjects.end(),
                               [&cleanName](const RecentProject& project) {
                                   return project.name == cleanName;
                               }),
                recentProjects.end());
        recentProjects.insert(recentProjects.begin(), {cleanName, currentDateText()});
        if (recentProjects.size() > 8) recentProjects.pop_back();
        saveRecentProjects();
        refreshRecentProjectTextures();
    }

    void beginNewProject() {
        projectNameBuffer.clear();
        currentProjectName.clear();
        projectNameActive = true;
        SDL_StartTextInput();
        currentState = AppState::NEW_PROJECT_DIALOG;
    }

    void finishNewProject() {
        currentProjectName = projectNameBuffer.empty() ? "Untitled_Project" : projectNameBuffer;
        addRecentProject(currentProjectName);
        projectNameActive = false;
        SDL_StopTextInput();
        currentState = AppState::WORKSPACE;
    }

public:
    AppManager(SDL_Renderer* renderer) : isRunning(true), currentState(AppState::MAIN_MENU) {
        rendererRef = renderer;
        recentProjectsTitle = nullptr;
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
        btnRun = new Button(185, 5, 65, 30, {46, 204, 113, 255}, white, "Run", statusFont, renderer);
        btnPause = new Button(255, 5, 70, 30, {241, 196, 15, 255}, white, "Pause", statusFont, renderer);
        btnStop = new Button(330, 5, 65, 30, {231, 76, 60, 255}, white, "Stop", statusFont, renderer);
        btnAddSelected = new Button(520, 490, 130, 35, {46, 204, 113, 255}, white, "Add Device", statusFont, renderer);
        btnCloseDialog = new Button(660, 490, 90, 35, {231, 76, 60, 255}, white, "Close", statusFont, renderer);

        library = {
                {"GND", "Ground (0V)", "Power Sources", "Circuit voltage reference"},
                {"DC_SRC", "DC Voltage Source", "Power Sources", "Ideal 5V DC source"},
                {"BATTERY", "Battery", "Power Sources", "Ideal battery source"},
                {"CLOCK", "Clock Generator", "Power Sources", "Periodic digital clock"},
                {"RES", "10k Resistor", "Analog", "Ohmic resistor"},
                {"CAP", "100uF Capacitor", "Analog", "Energy storage capacitor"},
                {"IND", "1mH Inductor", "Analog", "Energy storage inductor"},
                {"DIODE", "1N4148 Diode", "Analog", "Switching diode"},
                {"SWITCH", "Switch", "Interactive", "Persistent open/closed switch"},
                {"BUTTON", "Push Button", "Interactive", "Momentary HIGH while pressed"},
                {"LED", "Red LED", "Interactive", "LED output indicator"},
                {"SEG7", "7-Segment Display", "Interactive", "Seven segment digital display"},
                {"AND_GATE", "AND Gate", "Digital", "2-input AND gate"},
                {"NAND_GATE", "NAND Gate", "Digital", "2-input NAND gate"},
                {"XOR_GATE", "XOR Gate", "Digital", "2-input XOR gate"},
                {"NOT_GATE", "NOT Gate", "Digital", "Inverter"},
                {"OR_GATE", "OR Gate", "Digital", "2-input OR gate"},
                {"DFF", "D Flip-Flop", "Digital", "Rising-edge triggered D flip-flop"}
        };

        loadRecentProjects();
        refreshRecentProjectTextures();
        projectNameBuffer.clear();
        currentProjectName.clear();
    }

    ~AppManager() {
        SDL_StopTextInput();
        delete btnNewProject; delete btnA4; delete btnA3; delete btnCustom; delete btnBackToMenu;
        delete btnPickDevices; delete btnRun; delete btnPause; delete btnStop; delete btnAddSelected; delete btnCloseDialog;
        SDL_DestroyTexture(recentProjectsTitle);
        for (auto tex : recentTextTextures) SDL_DestroyTexture(tex);
        if (mainFont) TTF_CloseFont(mainFont);
        if (statusFont) TTF_CloseFont(statusFont);
        if (smallFont) TTF_CloseFont(smallFont);
    }

    LogicState resolveWireInput(int componentIndex, int pinIndex) const {
        LogicState resolved = LogicState::UNDEFINED;
        bool found = false;
        bool conflict = false;

        for (const auto& wire : wires) {
            int otherComponent = -1;
            int otherPin = -1;

//////////  بخش 07 - مدیریت AppManager
            if (wire.endComponent == componentIndex && wire.endPin == pinIndex) {
                otherComponent = wire.startComponent;
                otherPin = wire.startPin;
            } else if (wire.startComponent == componentIndex && wire.startPin == pinIndex) {
                otherComponent = wire.endComponent;
                otherPin = wire.endPin;
            } else {
                continue;
            }

            if (otherComponent < 0 || otherComponent >= static_cast<int>(canvasComponents.size()))
                continue;

            LogicState signal = canvasComponents[otherComponent].outputForPin(otherPin);
            if (signal == LogicState::UNDEFINED) continue;

            if (!found) {
                resolved = signal;
                found = true;
            } else if (resolved != signal) {
                conflict = true;
            }
        }

        if (conflict) return LogicState::UNDEFINED;
        return found ? resolved : LogicState::UNDEFINED;
    }

    LogicState logicAnd(LogicState a, LogicState b) const {
        if (a == LogicState::UNDEFINED || b == LogicState::UNDEFINED)
            return LogicState::UNDEFINED;
        return (a == LogicState::HIGH && b == LogicState::HIGH)
               ? LogicState::HIGH : LogicState::LOW;
    }

    LogicState logicOr(LogicState a, LogicState b) const {
        if (a == LogicState::UNDEFINED || b == LogicState::UNDEFINED)
            return LogicState::UNDEFINED;
        return (a == LogicState::HIGH || b == LogicState::HIGH)
               ? LogicState::HIGH : LogicState::LOW;
    }

    LogicState logicXor(LogicState a, LogicState b) const {
        if (a == LogicState::UNDEFINED || b == LogicState::UNDEFINED)
            return LogicState::UNDEFINED;
        return (a != b) ? LogicState::HIGH : LogicState::LOW;
    }

    LogicState logicNot(LogicState a) const {
        if (a == LogicState::UNDEFINED) return LogicState::UNDEFINED;
        return a == LogicState::HIGH ? LogicState::LOW : LogicState::HIGH;
    }

    void addSimulationLog(const std::string& message) {
        simulationLog.push_back(message);
        if (simulationLog.size() > 6) {
            simulationLog.erase(simulationLog.begin());
        }
        std::cout << message << std::endl;
    }

    void saveUndoState() {
        CircuitSnapshot snapshot;
        snapshot.components = canvasComponents;
        snapshot.wires = wires;
        snapshot.junctions = junctions;
        undoStack.push_back(std::move(snapshot));
        if (undoStack.size() > MAX_UNDO) {
            undoStack.erase(undoStack.begin());
        }
    }

    bool undoLastAction() {
        if (undoStack.empty()) {
            addSimulationLog("Undo: nothing to undo.");
            return false;
        }

        CircuitSnapshot snapshot = std::move(undoStack.back());
        undoStack.pop_back();
        canvasComponents = std::move(snapshot.components);
        wires = std::move(snapshot.wires);
        junctions = std::move(snapshot.junctions);
        rebuildAllWirePaths();
        cancelWiring();
        addSimulationLog("Undo: last circuit action restored.");
        return true;
    }

    void openProperties(int componentIndex) {
        if (componentIndex < 0 || componentIndex >= static_cast<int>(canvasComponents.size())) {
            return;
        }

        propertiesTargetIndex = componentIndex;
        propertiesTarget = &canvasComponents[componentIndex];
        propertyLabelBuffer = propertiesTarget->name;
        propertyValueBuffer = propertiesTarget->value;
        propertiesField = 0;
        showProperties = true;
        SDL_StartTextInput();
        addSimulationLog("Properties opened for " + propertiesTarget->name + ".");
    }
//////////  بخش 08 - Library و پروژه‌های اخیر

    void applyProperties() {
        if (propertiesTargetIndex < 0 ||
            propertiesTargetIndex >= static_cast<int>(canvasComponents.size())) {
            return;
        }

        saveUndoState();
        CircuitComponent& comp = canvasComponents[propertiesTargetIndex];
        comp.name = propertyLabelBuffer.empty() ? comp.name : propertyLabelBuffer;
        comp.value = propertyValueBuffer;

        showProperties = false;
        propertiesTarget = nullptr;
        propertiesTargetIndex = -1;
        SDL_StopTextInput();
        addSimulationLog("Properties updated.");
    }

    void closeProperties() {
        showProperties = false;
        propertiesTarget = nullptr;
        propertiesTargetIndex = -1;
        SDL_StopTextInput();
    }

    void setSimulationMode(SimulationMode mode) {
        simulationMode = mode;
        if (mode == SimulationMode::RUNNING) {
            addSimulationLog("Simulation RUN.");
        } else if (mode == SimulationMode::PAUSED) {
            addSimulationLog("Simulation PAUSED.");
        } else {
            addSimulationLog("Simulation STOPPED.");
        }
    }

    bool tryGetLogicAtCursor(LogicState& state) const {
        int pinComponent = -1;
        int pinIndex = -1;
        if (findPinAt(worldX, worldY, pinComponent, pinIndex)) {
            if (pinComponent >= 0 && pinComponent < static_cast<int>(canvasComponents.size())) {
                const CircuitComponent& c = canvasComponents[pinComponent];
                if (c.isOutputPin(pinIndex)) {
                    state = c.outputForPin(pinIndex);
                    return true;
                }
                if (c.isInputPin(pinIndex)) {
                    state = (pinIndex < static_cast<int>(c.pinLogic.size()))
                            ? c.pinLogic[pinIndex] : LogicState::UNDEFINED;
                    return true;
                }
            }
        }

        int wireIndex = findWireAt(worldX, worldY);
        if (wireIndex >= 0 && wireIndex < static_cast<int>(wires.size())) {
            const Wire& wire = wires[wireIndex];
            if (wire.startComponent >= 0 && wire.startComponent < static_cast<int>(canvasComponents.size())) {
                state = canvasComponents[wire.startComponent].outputForPin(wire.startPin);
                return true;
            }
        }

        return false;
    }

    float logicVoltage(LogicState state) const {
        if (state == LogicState::HIGH) return 5.0f;
        if (state == LogicState::LOW) return 0.0f;
        return 2.5f;
    }

    void simulatePart6() {
        if (simulationMode == SimulationMode::STOPPED) {
            return;
        }

        if (simulationMode == SimulationMode::PAUSED && !stepRequested) {
            return;
        }

        Uint32 now = SDL_GetTicks();

        for (auto& comp : canvasComponents) {
            if (comp.type == "CLOCK") {
                if (now - comp.lastClockTick >= 500) {
                    comp.outputHigh = !comp.outputHigh;
                    comp.lastClockTick = now;
                }
            }
        }

//////////  بخش 09 - منطق اتصال سیم‌ها
        for (auto& comp : canvasComponents) {
            if (comp.type == "LED" || comp.type == "SEG7") {
                for (int pi = 0; pi < static_cast<int>(comp.pins.size()); ++pi) {
                    if (comp.isInputPin(pi))
                        comp.setInput(pi, LogicState::UNDEFINED);
                }
            }
        }

        for (int iteration = 0; iteration < 3; ++iteration) {
            for (int ci = 0; ci < static_cast<int>(canvasComponents.size()); ++ci) {
                auto& comp = canvasComponents[ci];
                for (int pi = 0; pi < static_cast<int>(comp.pins.size()); ++pi) {
                    if (comp.isInputPin(pi)) {
                        comp.setInput(pi, resolveWireInput(ci, pi));
                    }
                }
            }

            for (auto& comp : canvasComponents) {
                if (comp.type == "AND_GATE" || comp.type == "NAND_GATE" ||
                    comp.type == "OR_GATE" || comp.type == "XOR_GATE") {
                    LogicState a = comp.pinLogic.size() > 0 ? comp.pinLogic[0] : LogicState::UNDEFINED;
                    LogicState b = comp.pinLogic.size() > 1 ? comp.pinLogic[1] : LogicState::UNDEFINED;
                    LogicState out = LogicState::UNDEFINED;

                    if (comp.type == "AND_GATE") out = logicAnd(a, b);
                    else if (comp.type == "NAND_GATE") out = logicNot(logicAnd(a, b));
                    else if (comp.type == "OR_GATE") out = logicOr(a, b);
                    else if (comp.type == "XOR_GATE") out = logicXor(a, b);

                    comp.pinLogic[2] = out;
                    comp.hasUndefinedWarning = (out == LogicState::UNDEFINED);
                }
                else if (comp.type == "NOT_GATE") {
                    LogicState a = comp.pinLogic.size() > 0 ? comp.pinLogic[0] : LogicState::UNDEFINED;
                    comp.pinLogic[1] = logicNot(a);
                    comp.hasUndefinedWarning = (comp.pinLogic[1] == LogicState::UNDEFINED);
                }
            }
        }

        for (auto& comp : canvasComponents) {
            if (comp.type == "DFF") {
                LogicState d = comp.pinLogic.size() > 0 ? comp.pinLogic[0] : LogicState::UNDEFINED;
                LogicState clk = comp.pinLogic.size() > 1 ? comp.pinLogic[1] : LogicState::UNDEFINED;

                if (clk == LogicState::UNDEFINED || d == LogicState::UNDEFINED) {
                    comp.hasUndefinedWarning = true;
                } else {
                    bool rising = (comp.previousClockState == LogicState::LOW &&
                                   clk == LogicState::HIGH);
                    if (rising) {
                        comp.storedLogicState = d;
                        addSimulationLog("DFF " + comp.name + " captured D on rising edge.");
                    }
                    comp.hasUndefinedWarning = false;
                }
                comp.previousClockState = clk;
                comp.pinLogic[2] = comp.storedLogicState;
            }
        }

        for (auto& comp : canvasComponents) {
            if (comp.type == "LED") {
                LogicState in = comp.pinLogic.empty() ? LogicState::UNDEFINED : comp.pinLogic[0];
                comp.outputHigh = (in == LogicState::HIGH);
                comp.hasUndefinedWarning = (in == LogicState::UNDEFINED);
            } else if (comp.type == "SEG7") {
                bool anyHigh = false;
                bool undefined = false;
                for (int i = 0; i < 7 && i < static_cast<int>(comp.pinLogic.size()); ++i) {
                    anyHigh = anyHigh || comp.pinLogic[i] == LogicState::HIGH;
                    undefined = undefined || comp.pinLogic[i] == LogicState::UNDEFINED;
                }
                comp.outputHigh = anyHigh;
                comp.hasUndefinedWarning = undefined;
            }
        }

        if (stepRequested) {
            stepRequested = false;
            simulationMode = SimulationMode::PAUSED;
            addSimulationLog("Single simulation step executed.");
        }
    }

    std::string logicStateText(LogicState state) const {
        if (state == LogicState::HIGH) return "HIGH";
        if (state == LogicState::LOW) return "LOW";
        return "UNDEFINED";
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
//////////  بخش 10 - Properties و Undo و Log

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

//////////  بخش 11 - موتور شبیه‌سازی دیجیتال
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

//////////  بخش 12 - Probe و ولتاژ
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

            if (e.type == SDL_KEYDOWN && currentState == AppState::WORKSPACE) {
                const SDL_Keymod mods = SDL_GetModState();
                if ((mods & KMOD_CTRL) && e.key.keysym.sym == SDLK_z) {
                    undoLastAction();
                } else if (e.key.keysym.sym == SDLK_F10) {
                    stepRequested = true;
                    simulationMode = SimulationMode::PAUSED;
                    addSimulationLog("Step requested.");
                } else {
                    switch (e.key.keysym.sym) {
                        case SDLK_DELETE:
                            if (isWiring) {
                                cancelWiring();
                                break;
                            }

                            if (hoveredWireIndex != -1) {
                                saveUndoState();
                                deleteWireAtCursor(worldX, worldY);
                                break;
                            }

                            for (int i = static_cast<int>(canvasComponents.size()) - 1; i >= 0; --i) {
                                if (canvasComponents[i].isSelected) {
                                    saveUndoState();
                                    removeWiresConnectedToComponent(i);
                                    canvasComponents.erase(canvasComponents.begin() + i);
                                }
                            }
                            rebuildAllWirePaths();
                            break;
                        case SDLK_r:
                            for (auto& comp : canvasComponents) if (comp.isSelected) comp.rotate();
                            break;
                        case SDLK_m:
                            for (auto& comp : canvasComponents) if (comp.isSelected) comp.mirror(true);
                            break;
                        case SDLK_ESCAPE:
                            deviceToPlace = "";
                            selectedActiveIndex = -1;
                            cancelWiring();
                            for (auto& c : canvasComponents) c.isSelected = false;
                            break;
                    }
                }
            }

            if (projectNameActive && currentState == AppState::NEW_PROJECT_DIALOG) {
                if (e.type == SDL_TEXTINPUT) {
                    projectNameBuffer += e.text.text;
                    continue;
                }
                if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_BACKSPACE) {
                    if (!projectNameBuffer.empty()) projectNameBuffer.pop_back();
                    continue;
                }
                if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN) {
                    finishNewProject();
                    continue;
                }
                if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                    projectNameBuffer.clear();
                    projectNameActive = false;
                    SDL_StopTextInput();
                    currentState = AppState::MAIN_MENU;
                    continue;
                }
            }

            if (showProperties && currentState == AppState::WORKSPACE) {
                if (e.type == SDL_TEXTINPUT) {
                    if (propertiesField == 0) propertyLabelBuffer += e.text.text;
                    else propertyValueBuffer += e.text.text;
                    continue;
                }
                if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_BACKSPACE) {
                    std::string& active = (propertiesField == 0) ? propertyLabelBuffer : propertyValueBuffer;
                    if (!active.empty()) active.pop_back();
                    continue;
                }
                if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN) {
                    applyProperties();
                    continue;
                }
                if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                    closeProperties();
                    continue;
                }
            }

//////////  بخش 13 - توابع هندسی پایه و سیم
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
                    if (showProperties && currentState == AppState::WORKSPACE) {
                        SDL_Point point{mouseX, mouseY};
                        SDL_Rect labelBox{300, 190, 350, 36};
                        SDL_Rect valueBox{300, 260, 350, 36};
                        SDL_Rect okBox{300, 335, 160, 42};
                        SDL_Rect cancelBox{490, 335, 160, 42};

                        if (SDL_PointInRect(&point, &labelBox)) propertiesField = 0;
                        else if (SDL_PointInRect(&point, &valueBox)) propertiesField = 1;
                        else if (SDL_PointInRect(&point, &okBox)) applyProperties();
                        else if (SDL_PointInRect(&point, &cancelBox)) closeProperties();
                        continue;
                    }

                    if (currentState == AppState::MAIN_MENU) {
                        if (btnNewProject->isClicked(mouseX, mouseY)) beginNewProject();
                    }
                    else if (currentState == AppState::NEW_PROJECT_DIALOG) {
                        if (btnA4->isClicked(mouseX, mouseY) ||
                            btnA3->isClicked(mouseX, mouseY) ||
                            btnCustom->isClicked(mouseX, mouseY)) {
                            finishNewProject();
                        }
                    }
                    else if (currentState == AppState::WORKSPACE) {

                        if (mouseX > SIDEBAR_WIDTH) {
                            for (auto it = canvasComponents.rbegin(); it != canvasComponents.rend(); ++it) {
                                if (it->contains(worldX, worldY)) {
                                    if (it->type == "SWITCH") {
                                        saveUndoState();
                                        it->switchOn = !it->switchOn;
                                        it->value = it->switchOn ? "ON" : "OFF";
                                        addSimulationLog(it->name + (it->switchOn ? " -> ON" : " -> OFF"));
                                        it->isSelected = true;
                                        break;
                                    }
                                    if (it->type == "BUTTON") {
                                        it->buttonPressed = true;
                                        it->isSelected = true;
                                        addSimulationLog(it->name + " pressed.");
                                        break;
                                    }
                                }
                            }
                        }

                        if (btnRun->isClicked(mouseX, mouseY)) {
                            setSimulationMode(SimulationMode::RUNNING);
                        }
                        else if (btnPause->isClicked(mouseX, mouseY)) {
                            setSimulationMode(SimulationMode::PAUSED);
                        }
                        else if (btnStop->isClicked(mouseX, mouseY)) {
                            setSimulationMode(SimulationMode::STOPPED);
                        }
                        else if (btnBackToMenu->isClicked(mouseX, mouseY)) {
                            cancelWiring();
                            projectNameActive = false;
                            SDL_StopTextInput();
                            currentState = AppState::MAIN_MENU;
                        }
                        else if (btnPickDevices->isClicked(mouseX, mouseY)) {
                            currentState = AppState::PICK_DEVICES_DIALOG;
                            SDL_StartTextInput();
                        }
//////////  بخش 14 - تشخیص Wire و Pin

                        else if (mouseX < SIDEBAR_WIDTH && mouseY > 40 && mouseY < 570) {
                            int idx = (mouseY - 50) / 30;
                            if (idx >= 0 && idx < (int)activeDevices.size()) {
                                selectedActiveIndex = idx;
                                deviceToPlace = activeDevices[idx].id;
                                for (auto& c : canvasComponents) c.isSelected = false;
                            }
                        }

                        else if (mouseX > SIDEBAR_WIDTH) {
                            bool isDoubleClick = (SDL_GetTicks() - lastClickTime < 300);
                            lastClickTime = SDL_GetTicks();

                            if (!deviceToPlace.empty()) {
                                saveUndoState();
                                std::string autoName = deviceToPlace.substr(0, 1) + std::to_string(componentCounter++);
                                canvasComponents.push_back(CircuitComponent(deviceToPlace, snappedWorldX, snappedWorldY, autoName));
                                deviceToPlace = "";
                                selectedActiveIndex = -1;
                            }
                            else {

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

                                                saveUndoState();
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

                                    if (hoveredWireIndex != -1) {
                                        std::cout
                                                << "Wire selected. Press Delete to remove it."
                                                << std::endl;
                                        break;
                                    }
                                }

                                bool clickedOnComponent = false;
                                for (auto it = canvasComponents.rbegin(); it != canvasComponents.rend(); ++it) {
                                    if (it->contains(worldX, worldY)) {
                                        clickedOnComponent = true;

//////////  بخش 15 - رویدادهای صفحه‌کلید
                                        if (isDoubleClick) {
                                            int targetIndex = static_cast<int>(&(*it) - canvasComponents.data());
                                            openProperties(targetIndex);
                                            break;
                                        }

                                        const Uint8* state = SDL_GetKeyboardState(NULL);
                                        if (!state[SDL_SCANCODE_LSHIFT] && !it->isSelected) {
                                            for (auto& c : canvasComponents) c.isSelected = false;
                                        }
                                        it->isSelected = true;

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
                            std::vector<std::string> cats = {"All", "Analog", "Digital", "Power Sources", "Interactive", "Optoelectronics"};
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
                                deviceToPlace = "";
                            }
                        }
                    }
                }
            }

            if (e.type == SDL_MOUSEBUTTONUP) {
                if (e.button.button == SDL_BUTTON_LEFT) {
                    for (auto& comp : canvasComponents) {
                        if (comp.type == "BUTTON") comp.buttonPressed = false;
                    }
                    isDragging = false;

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
//////////  بخش 16 - رویدادهای موس و تعامل قطعات

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
            simulatePart6();
            wireAnimationOffset += (simulationMode == SimulationMode::RUNNING) ? 0.8f : 0.0f;
            if (wireAnimationOffset > 10000.0f) wireAnimationOffset = 0.0f;
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

            renderText(renderer, mainFont, "Project Name", 250, 48, {255, 255, 255, 255});
            SDL_Rect nameBox = {250, 85, 400, 40};
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &nameBox);
            SDL_SetRenderDrawColor(renderer, 52, 152, 219, 255);
            SDL_RenderDrawRect(renderer, &nameBox);
            std::string shownName = projectNameBuffer + "|";
            renderText(renderer, statusFont, shownName, nameBox.x + 10, nameBox.y + 10, {20, 20, 20, 255});
            renderText(renderer, smallFont, "Enter project name then choose a page size", 250, 130, {210, 210, 210, 255});

            btnA4->draw(renderer);
            btnA3->draw(renderer);
            btnCustom->draw(renderer);
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

            rebuildAllWirePaths();

            for (size_t wireIndex = 0; wireIndex < wires.size(); ++wireIndex) {
                const Wire& wire = wires[wireIndex];

                if (wire.points.size() < 2) {
                    continue;
                }

                LogicState wireState = LogicState::UNDEFINED;
                if (wire.startComponent >= 0 && wire.startComponent < static_cast<int>(canvasComponents.size())) {
                    wireState = canvasComponents[wire.startComponent].outputForPin(wire.startPin);
                }

                if (static_cast<int>(wireIndex) == hoveredWireIndex) {
                    SDL_SetRenderDrawColor(renderer, 230, 126, 34, 255);
                } else if (wireState == LogicState::HIGH) {
                    SDL_SetRenderDrawColor(renderer, 210, 50, 50, 255);
                } else if (wireState == LogicState::LOW) {
                    SDL_SetRenderDrawColor(renderer, 50, 100, 220, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer, 110, 110, 110, 255);
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

//////////  بخش 17 - جابه‌جایی و Zoom
                    if (simulationMode == SimulationMode::RUNNING && wireState != LogicState::UNDEFINED) {
                        float phase = std::fmod(wireAnimationOffset + static_cast<float>(p) * 12.0f, 24.0f) / 24.0f;
                        int ax = x1 + static_cast<int>((x2 - x1) * phase);
                        int ay = y1 + static_cast<int>((y2 - y1) * phase);
                        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                        SDL_RenderDrawPoint(renderer, ax, ay);
                        SDL_RenderDrawPoint(renderer, ax + 1, ay);
                    }
                }
            }

            for (size_t compIndex = 0; compIndex < canvasComponents.size(); ++compIndex) {
                const auto& comp = canvasComponents[compIndex];
                float currentW = (comp.rotation % 180 == 0) ? comp.width : comp.height;
                float currentH = (comp.rotation % 180 == 0) ? comp.height : comp.width;

                int screenX = (int)(comp.x * zoom + cameraX);
                int screenY = (int)(comp.y * zoom + cameraY);
                int screenW = (int)(currentW * zoom);
                int screenH = (int)(currentH * zoom);

                SDL_Rect compRect = { screenX - screenW / 2, screenY - screenH / 2, screenW, screenH };

                SDL_SetRenderDrawColor(renderer, 230, 235, 240, 255);
                SDL_RenderFillRect(renderer, &compRect);

                if ((comp.type == "LED" || comp.type == "SEG7") && comp.outputHigh) {
                    SDL_SetRenderDrawColor(renderer, 255, 230, 80, 100);
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                    SDL_RenderFillRect(renderer, &compRect);
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                }

                if ((comp.type == "SWITCH" && comp.switchOn) ||
                    (comp.type == "BUTTON" && comp.buttonPressed)) {
                    SDL_SetRenderDrawColor(renderer, 46, 204, 113, 255);
                    SDL_RenderDrawLine(renderer, screenX - screenW / 3, screenY,
                                       screenX + screenW / 3, screenY);
                }

                if (comp.hasUndefinedWarning) {
                    SDL_SetRenderDrawColor(renderer, 231, 76, 60, 255);
                    SDL_RenderDrawRect(renderer, &compRect);
                }

                if (comp.isSelected) {
                    SDL_SetRenderDrawColor(renderer, 231, 76, 60, 255);
                    SDL_RenderDrawRect(renderer, &compRect);
                    SDL_Rect innerRect = { compRect.x+1, compRect.y+1, compRect.w-2, compRect.h-2 };
                    SDL_RenderDrawRect(renderer, &innerRect);
                } else {
                    SDL_SetRenderDrawColor(renderer, 52, 152, 219, 255);
                    SDL_RenderDrawRect(renderer, &compRect);
                }

                renderText(renderer, statusFont, comp.name, screenX - 10, screenY - 20, {20, 20, 20, 255});
                renderText(renderer, smallFont, comp.value, screenX - 10, screenY + 5, {100, 100, 100, 255});

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

                    SDL_RenderDrawLine(
                            renderer,
                            screenX, screenY,
                            pinScreenX, pinScreenY);
                }
            }

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

//////////  بخش 18 - رندر سیم‌ها و قطعات
                int cx = static_cast<int>(corner.x * zoom + cameraX);
                int cy = static_cast<int>(corner.y * zoom + cameraY);

                int ex = static_cast<int>(end.x * zoom + cameraX);
                int ey = static_cast<int>(end.y * zoom + cameraY);

                SDL_SetRenderDrawColor(renderer, 52, 152, 219, 255);
                SDL_RenderDrawLine(renderer, sx, sy, cx, cy);
                SDL_RenderDrawLine(renderer, cx, cy, ex, ey);
            }

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

            if (!deviceToPlace.empty() && mouseX > SIDEBAR_WIDTH) {
                int ghostX = (int)(snappedWorldX * zoom + cameraX);
                int ghostY = (int)(snappedWorldY * zoom + cameraY);
                SDL_Rect ghostR = { ghostX - 40, ghostY - 20, 80, 40 };
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 46, 204, 113, 100);
                SDL_RenderFillRect(renderer, &ghostR);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                renderText(renderer, statusFont, deviceToPlace, ghostX - 20, ghostY - 10, {30, 150, 30, 255});
            }

            if (mouseX > SIDEBAR_WIDTH) {
                int screenSnapX = (int)(snappedWorldX * zoom + cameraX);
                int screenSnapY = (int)(snappedWorldY * zoom + cameraY);
                SDL_Rect snapCursor = {screenSnapX - 3, screenSnapY - 3, 6, 6};
                SDL_SetRenderDrawColor(renderer, 231, 76, 60, 255);
                SDL_RenderDrawRect(renderer, &snapCursor);
            }

            SDL_Rect sidebar = {0, 0, SIDEBAR_WIDTH, 600 - 30};
            SDL_SetRenderDrawColor(renderer, 220, 225, 230, 255);
            SDL_RenderFillRect(renderer, &sidebar);
            SDL_SetRenderDrawColor(renderer, 160, 165, 170, 255);
            SDL_RenderDrawLine(renderer, SIDEBAR_WIDTH, 0, SIDEBAR_WIDTH, 600 - 30);

            btnBackToMenu->draw(renderer);
            btnPickDevices->draw(renderer);
            btnRun->draw(renderer);
            btnPause->draw(renderer);
            btnStop->draw(renderer);

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

//////////  بخش 19 - رندر Status و ابزارها
            std::string statusText =
                    "Project: " + (currentProjectName.empty() ? std::string("Untitled_Project") : currentProjectName) +
                    "  |  Zoom: " + std::to_string((int)(zoom * 100)) +
                    "%  |  X: " + std::to_string((int)snappedWorldX) +
                    " Y: " + std::to_string((int)snappedWorldY) +
                    "  |  Wires: " + std::to_string(wires.size()) +
                    "  |  " + modeText;
            renderText(renderer, statusFont, statusText, 15, 600 - 23, {50, 50, 50, 255});

            LogicState probeState = LogicState::UNDEFINED;
            if (tryGetLogicAtCursor(probeState)) {
                std::string probe = "Probe: " + logicStateText(probeState) +
                                    "  " + std::to_string(logicVoltage(probeState)) + " V";
                renderText(renderer, smallFont, probe, 400, 600 - 23, {40, 40, 40, 255});
            }

            bool undefinedFound = false;
            for (const auto& comp : canvasComponents) {
                if (comp.hasUndefinedWarning) {
                    undefinedFound = true;
                    break;
                }
            }
            if (undefinedFound) {
                renderText(renderer, smallFont, "Warning: Floating input detected.",
                           600, 600 - 23, {200, 50, 50, 255});
            }

            SDL_SetRenderDrawColor(renderer, 245, 245, 245, 230);
            SDL_Rect logRect = {650, 395, 235, 105};
            SDL_RenderFillRect(renderer, &logRect);
            SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
            SDL_RenderDrawRect(renderer, &logRect);
            renderText(renderer, smallFont, "Simulation Log", 660, 402, {60, 60, 60, 255});
            int logY = 420;
            int start = std::max(0, static_cast<int>(simulationLog.size()) - 4);
            for (int i = start; i < static_cast<int>(simulationLog.size()); ++i) {
                renderText(renderer, smallFont, simulationLog[i], 660, logY, {80, 80, 80, 255});
                logY += 18;
            }

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
                std::vector<std::string> categories = {"All", "Analog", "Digital", "Power Sources", "Interactive", "Optoelectronics"};
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
//////////  بخش 20 - رندر Properties و تابع main

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
        if (showProperties && currentState == AppState::WORKSPACE) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
            SDL_Rect shade = {0, 0, 900, 600};
            SDL_RenderFillRect(renderer, &shade);

            SDL_Rect modal = {220, 115, 460, 300};
            SDL_SetRenderDrawColor(renderer, 242, 244, 247, 255);
            SDL_RenderFillRect(renderer, &modal);
            SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255);
            SDL_RenderDrawRect(renderer, &modal);

            renderText(renderer, mainFont, "Component Properties", 245, 135, {30, 30, 30, 255});
            renderText(renderer, statusFont, "Label", 245, 180, {50, 50, 50, 255});
            renderText(renderer, statusFont, "Value", 245, 250, {50, 50, 50, 255});

            SDL_Rect labelBox = {300, 190, 350, 36};
            SDL_Rect valueBox = {300, 260, 350, 36};
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &labelBox);
            SDL_RenderFillRect(renderer, &valueBox);
            SDL_SetRenderDrawColor(renderer, propertiesField == 0 ? 52 : 180,
                                   propertiesField == 0 ? 152 : 180,
                                   propertiesField == 0 ? 219 : 180, 255);
            SDL_RenderDrawRect(renderer, &labelBox);
            SDL_SetRenderDrawColor(renderer, propertiesField == 1 ? 52 : 180,
                                   propertiesField == 1 ? 152 : 180,
                                   propertiesField == 1 ? 219 : 180, 255);
            SDL_RenderDrawRect(renderer, &valueBox);
            renderText(renderer, statusFont, propertyLabelBuffer + (propertiesField == 0 ? "|" : ""), 310, 198, {20,20,20,255});
            renderText(renderer, statusFont, propertyValueBuffer + (propertiesField == 1 ? "|" : ""), 310, 268, {20,20,20,255});

            SDL_Rect okBox = {300, 335, 160, 42};
            SDL_Rect cancelBox = {490, 335, 160, 42};
            SDL_SetRenderDrawColor(renderer, 46, 204, 113, 255);
            SDL_RenderFillRect(renderer, &okBox);
            SDL_SetRenderDrawColor(renderer, 231, 76, 60, 255);
            SDL_RenderFillRect(renderer, &cancelBox);
            renderText(renderer, statusFont, "Apply", 355, 345, {255,255,255,255});
            renderText(renderer, statusFont, "Cancel", 535, 345, {255,255,255,255});
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }

        SDL_RenderPresent(renderer);
    }

    bool getIsRunning() const { return isRunning; }
};

#undef main
int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window* window = SDL_CreateWindow("Proteus Simulator - Part 6",
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
