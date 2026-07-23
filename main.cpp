#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cctype>

// وضعیت‌های برنامه
enum class AppState {
    MAIN_MENU,
    NEW_PROJECT_DIALOG,
    WORKSPACE,
    PICK_DEVICES_DIALOG // پنجره کتابخانه قطعات (بخش سوم)
};

// ساختار تعریف یک قطعه الکترونیکی
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

    // دکمه‌های منوها
    Button* btnNewProject;
    Button* btnA4;
    Button* btnA3;
    Button* btnCustom;
    Button* btnBackToMenu;
    Button* btnPickDevices; // دکمه P برای باز کردن کتابخانه
    Button* btnAddSelected; // دکمه افزودن قطعه
    Button* btnCloseDialog; // دکمه بستن کتابخانه

    // اطلاعات منوی اصلی
    SDL_Texture* recentProjectsTitle;
    SDL_Rect titleRect;
    std::vector<SDL_Texture*> recentTextTextures;
    std::vector<SDL_Rect> recentTextRects;

    // متغیرهای دوربین و بوم طراحی (بخش دوم)
    float cameraX, cameraY;
    float zoom;
    bool isPanning;
    int panStartX, panStartY;
    float camStartX, camStartY;
    int mouseX, mouseY;
    float worldX, worldY;
    float snappedWorldX, snappedWorldY;
    const float GRID_SIZE = 30.0f;

    // ----- متغیرهای کتابخانه قطعات (بخش سوم) -----
    std::vector<ComponentDef> library;          // پایگاه داده تمام قطعات
    std::vector<ComponentDef> activeDevices;    // لیست قطعات فعال در پنل سمت چپ
    int selectedActiveIndex;                    // قطعه انتخاب شده در پنل سمت چپ

    std::string searchQuery;                    // متن جستجو
    std::string selectedCategory;               // دسته بندی انتخاب شده
    int selectedLibraryIndex;                   // قطعه انتخاب شده در پنجره کتابخانه

    const int SIDEBAR_WIDTH = 180;              // عرض پنل سمت چپ

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

    // تابع رسم گرافیکی نماد شماتیک قطعات (Schematic Preview)
    void drawSchematicSymbol(SDL_Renderer* renderer, const std::string& compId, int cx, int cy) {
        SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255); // رنگ قرمز/قهوه‌ای پروتیوسی

        if (compId == "RES") { // مقاومت
            SDL_RenderDrawLine(renderer, cx - 40, cy, cx - 20, cy);
            SDL_RenderDrawLine(renderer, cx + 20, cy, cx + 40, cy);
            SDL_Rect box = {cx - 20, cy - 8, 40, 16};
            SDL_RenderDrawRect(renderer, &box);
        }
        else if (compId == "CAP") { // خازن
            SDL_RenderDrawLine(renderer, cx - 30, cy, cx - 8, cy);
            SDL_RenderDrawLine(renderer, cx + 8, cy, cx + 30, cy);
            SDL_RenderDrawLine(renderer, cx - 8, cy - 15, cx - 8, cy + 15);
            SDL_RenderDrawLine(renderer, cx + 8, cy - 15, cx + 8, cy + 15);
        }
        else if (compId == "DC_SRC") { // منبع DC
            SDL_RenderDrawLine(renderer, cx - 30, cy, cx - 15, cy);
            SDL_RenderDrawLine(renderer, cx + 15, cy, cx + 30, cy);
            // رسم دایره تقریبی
            for (int w = 0; w < 360; w += 20) {
                float rad1 = w * 3.14159f / 180.0f;
                float rad2 = (w + 20) * 3.14159f / 180.0f;
                SDL_RenderDrawLine(renderer, cx + (int)(15 * cos(rad1)), cy + (int)(15 * sin(rad1)),
                                   cx + (int)(15 * cos(rad2)), cy + (int)(15 * sin(rad2)));
            }
            renderText(renderer, statusFont, "+", cx - 10, cy - 12, {180, 40, 40, 255});
            renderText(renderer, statusFont, "-", cx + 3, cy - 12, {180, 40, 40, 255});
        }
        else if (compId == "DIODE" || compId == "LED") { // دیود و ال‌ای‌دی
            SDL_RenderDrawLine(renderer, cx - 30, cy, cx - 10, cy);
            SDL_RenderDrawLine(renderer, cx + 10, cy, cx + 30, cy);
            // مثلثة دیود
            SDL_RenderDrawLine(renderer, cx - 10, cy - 12, cx - 10, cy + 12);
            SDL_RenderDrawLine(renderer, cx - 10, cy - 12, cx + 10, cy);
            SDL_RenderDrawLine(renderer, cx - 10, cy + 12, cx + 10, cy);
            SDL_RenderDrawLine(renderer, cx + 10, cy - 12, cx + 10, cy + 12);
            if (compId == "LED") { // فلش‌های نور برای LED
                SDL_RenderDrawLine(renderer, cx, cy - 12, cx + 8, cy - 20);
                SDL_RenderDrawLine(renderer, cx + 5, cy - 12, cx + 13, cy - 20);
            }
        }
        else if (compId == "GND") { // زمین
            SDL_RenderDrawLine(renderer, cx, cy - 20, cx, cy);
            SDL_RenderDrawLine(renderer, cx - 15, cy, cx + 15, cy);
            SDL_RenderDrawLine(renderer, cx - 10, cy + 5, cx + 10, cy + 5);
            SDL_RenderDrawLine(renderer, cx - 5, cy + 10, cx + 5, cy + 10);
        }
        else { // پیش‌فرض برای دیجیتال و غیره
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

        // تعریف پایگاه داده قطعات کتابخانه
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

    // گرفتن لیست قطعات فیلتر شده (بر اساس دسته و متن جستجو)
    std::vector<ComponentDef> getFilteredLibrary() {
        std::vector<ComponentDef> result;
        std::string queryLower = searchQuery;
        std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(), ::tolower);

        for (const auto& comp : library) {
            bool matchesCategory = (selectedCategory == "All" || comp.category == selectedCategory);

            std::string nameLower = comp.name;
            std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

            bool matchesQuery = queryLower.empty() || (nameLower.find(queryLower) != std::string::npos);

            if (matchesCategory && matchesQuery) {
                result.push_back(comp);
            }
        }
        return result;
    }

    void handleEvents() {
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) isRunning = false;

            // ورود متن در کادر جستجو (Real-time Search Bar)
            if (currentState == AppState::PICK_DEVICES_DIALOG) {
                if (e.type == SDL_TEXTINPUT) {
                    searchQuery += e.text.text;
                    selectedLibraryIndex = -1;
                }
                else if (e.type == SDL_KEYDOWN) {
                    if (e.key.keysym.sym == SDLK_BACKSPACE && !searchQuery.empty()) {
                        searchQuery.pop_back();
                        selectedLibraryIndex = -1;
                    }
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
                        if (btnBackToMenu->isClicked(mouseX, mouseY)) currentState = AppState::MAIN_MENU;
                        else if (btnPickDevices->isClicked(mouseX, mouseY)) {
                            currentState = AppState::PICK_DEVICES_DIALOG;
                            SDL_StartTextInput(); // فعال کردن کیبورد برای جستجو
                        }

                        // کلیک روی لیست قطعات فعال در پنل سمت چپ
                        if (mouseX < SIDEBAR_WIDTH && mouseY > 40 && mouseY < 570) {
                            int idx = (mouseY - 50) / 30;
                            if (idx >= 0 && idx < (int)activeDevices.size()) {
                                selectedActiveIndex = idx;
                            }
                        }
                    }
                    else if (currentState == AppState::PICK_DEVICES_DIALOG) {
                        if (btnCloseDialog->isClicked(mouseX, mouseY)) {
                            currentState = AppState::WORKSPACE;
                            SDL_StopTextInput();
                        }

                        // کلیک روی منوی دسته‌بندی‌ها (سمت چپ پنجره)
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

                        // کلیک روی لیست قطعات فیلترشده (وسط پنجره)
                        auto filtered = getFilteredLibrary();
                        if (mouseX >= 250 && mouseX <= 500 && mouseY >= 160 && mouseY <= 470) {
                            int idx = (mouseY - 160) / 30;
                            if (idx >= 0 && idx < (int)filtered.size()) {
                                selectedLibraryIndex = idx;
                            }
                        }

                        // دکمه افزودن قطعه انتخاب شده به پنل فعال
                        if (btnAddSelected->isClicked(mouseX, mouseY) && selectedLibraryIndex != -1 && selectedLibraryIndex < (int)filtered.size()) {
                            activeDevices.push_back(filtered[selectedLibraryIndex]);
                            currentState = AppState::WORKSPACE;
                            SDL_StopTextInput();
                        }
                    }
                }
                else if (e.button.button == SDL_BUTTON_RIGHT) {
                    if (currentState == AppState::WORKSPACE) {
                        if (mouseX > SIDEBAR_WIDTH) { // Panning فقط خارج از پنل
                            isPanning = true; panStartX = mouseX; panStartY = mouseY;
                            camStartX = cameraX; camStartY = cameraY;
                        } else {
                            // کلیک راست در پنل سمت چپ برای حذف قطعه از لیست فعال
                            int idx = (mouseY - 50) / 30;
                            if (idx >= 0 && idx < (int)activeDevices.size()) {
                                activeDevices.erase(activeDevices.begin() + idx);
                                if (selectedActiveIndex >= (int)activeDevices.size()) selectedActiveIndex = -1;
                            }
                        }
                    }
                }
            }

            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT) isPanning = false;

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
            }
        }
    }

    void render(SDL_Renderer* renderer) {
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
            // ۱. پس‌زمینه بوم اصلی
            SDL_SetRenderDrawColor(renderer, 245, 247, 250, 255);
            SDL_RenderClear(renderer);

            // ۲. رسم شبکه (فقط سمت راست پنل)
            SDL_SetRenderDrawColor(renderer, 180, 185, 195, 255);
            float scaledGridSize = GRID_SIZE * zoom;
            float offsetX = std::fmod(cameraX, scaledGridSize);
            float offsetY = std::fmod(cameraY, scaledGridSize);
            if (offsetX < 0) offsetX += scaledGridSize;
            if (offsetY < 0) offsetY += scaledGridSize;

            for (float x = std::max((float)SIDEBAR_WIDTH, offsetX); x < 900; x += scaledGridSize)
                for (float y = offsetY; y < 600 - 30; y += scaledGridSize)
                    SDL_RenderDrawPoint(renderer, (int)x, (int)y);

            // ۳. نشانگر Snap to Grid
            if (mouseX > SIDEBAR_WIDTH) {
                int screenSnapX = (int)(snappedWorldX * zoom + cameraX);
                int screenSnapY = (int)(snappedWorldY * zoom + cameraY);
                SDL_Rect snapCursor = {screenSnapX - 3, screenSnapY - 3, 6, 6};
                SDL_SetRenderDrawColor(renderer, 231, 76, 60, 255);
                SDL_RenderDrawRect(renderer, &snapCursor);
            }

            // ۴. نوار سمت چپ (Active Devices Panel - پروتیوسی)
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

            // رندر لیست قطعات فعال در سمت چپ
            int devY = 70;
            for (size_t i = 0; i < activeDevices.size(); ++i) {
                SDL_Rect itemRect = {5, devY, SIDEBAR_WIDTH - 10, 26};
                if ((int)i == selectedActiveIndex) {
                    SDL_SetRenderDrawColor(renderer, 52, 152, 219, 255); // آبی
                    SDL_RenderFillRect(renderer, &itemRect);
                    renderText(renderer, statusFont, activeDevices[i].id, 10, devY + 4, {255, 255, 255, 255});
                } else {
                    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
                    SDL_RenderFillRect(renderer, &itemRect);
                    renderText(renderer, statusFont, activeDevices[i].id, 10, devY + 4, {40, 40, 40, 255});
                }
                devY += 30;
            }

            // ۵. نوار وضعیت پایین
            SDL_Rect statusBar = {0, 600 - 30, 900, 30};
            SDL_SetRenderDrawColor(renderer, 200, 205, 215, 255);
            SDL_RenderFillRect(renderer, &statusBar);
            SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
            SDL_RenderDrawLine(renderer, 0, 600 - 30, 900, 600 - 30);

            std::string statusText = "Zoom: " + std::to_string((int)(zoom * 100)) + "%   |   X: " +
                                     std::to_string((int)snappedWorldX) + "  Y: " + std::to_string((int)snappedWorldY) +
                                     "   |   (Right click on item in left panel to delete)";
            renderText(renderer, statusFont, statusText, 15, 600 - 23, {50, 50, 50, 255});

            // ----- ۶. پنجره مدال مدیریت کتابخانه قطعات (Pick Devices Dialog) -----
            if (currentState == AppState::PICK_DEVICES_DIALOG) {
                // تار کردن پس‌زمینه
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 120);
                SDL_Rect fullScreen = {0, 0, 900, 600};
                SDL_RenderFillRect(renderer, &fullScreen);

                // بدنه پنجره کتابخانه
                SDL_Rect dialog = {100, 50, 700, 490};
                SDL_SetRenderDrawColor(renderer, 240, 242, 245, 255);
                SDL_RenderFillRect(renderer, &dialog);
                SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
                SDL_RenderDrawRect(renderer, &dialog);

                // تیتر پنجره
                renderText(renderer, mainFont, "Pick Devices - Component Library", 120, 65, {30, 30, 30, 255});

                // ۱.۳ کادر جستجو (Search Bar)
                renderText(renderer, statusFont, "Keywords / Search:", 120, 105, {50, 50, 50, 255});
                SDL_Rect searchBox = {250, 100, 430, 28};
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderFillRect(renderer, &searchBox);
                SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
                SDL_RenderDrawRect(renderer, &searchBox);
                renderText(renderer, statusFont, searchQuery + "|", 255, 105, {20, 20, 20, 255});

                // ۲.۳ لیست دسته‌بندی‌ها (Category View)
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

                // ۳.۳ لیست قطعات فیلتر شده (Device Table)
                SDL_Rect tableBox = {250, 140, 250, 330};
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderFillRect(renderer, &tableBox);
                SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
                SDL_RenderDrawRect(renderer, &tableBox);

                auto filtered = getFilteredLibrary();
                if (filtered.empty()) {
                    // ۲.۳ پیام نبود قطعه
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

                // ۴.۳ پیش‌نمایش شماتیک (Schematic Preview Box)
                SDL_Rect previewBox = {515, 140, 265, 200};
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderFillRect(renderer, &previewBox);
                SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
                SDL_RenderDrawRect(renderer, &previewBox);
                renderText(renderer, smallFont, "Schematic Preview:", 520, 145, {120, 120, 120, 255});

                // توضیحات قطعه (Description)
                SDL_Rect descBox = {515, 350, 265, 120};
                SDL_SetRenderDrawColor(renderer, 250, 250, 250, 255);
                SDL_RenderFillRect(renderer, &descBox);
                SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
                SDL_RenderDrawRect(renderer, &descBox);

                if (selectedLibraryIndex != -1 && selectedLibraryIndex < (int)filtered.size()) {
                    // رسم پیش‌نمایش قطعه انتخاب شده
                    drawSchematicSymbol(renderer, filtered[selectedLibraryIndex].id, 515 + 132, 140 + 100);

                    // نمایش توضیحات
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

    SDL_Window* window = SDL_CreateWindow("Proteus Simulator - Components Library",
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