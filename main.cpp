#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <string>

// وضعیت‌های مختلف رابط کاربری
enum class AppState {
    MAIN_MENU,
    NEW_PROJECT_DIALOG,
    WORKSPACE
};

// ساختار ذخیره اطلاعات پروژه‌های اخیر
struct RecentProject {
    std::string name;
    std::string lastModified;
};

// کلاس دکمه با قابلیت رندر متن
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

        // ساخت تکسچر برای متن دکمه
        SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), textColor);
        textTexture = SDL_CreateTextureFromSurface(renderer, surface);

        // تنظیم موقعیت متن در مرکز دکمه
        textRect.w = surface->w;
        textRect.h = surface->h;
        textRect.x = rect.x + (rect.w - textRect.w) / 2;
        textRect.y = rect.y + (rect.h - textRect.h) / 2;

        SDL_FreeSurface(surface);
    }

    ~Button() {
        if (textTexture) SDL_DestroyTexture(textTexture);
    }

    void draw(SDL_Renderer* renderer) {
        SDL_SetRenderDrawColor(renderer, bgColor.r, bgColor.g, bgColor.b, 255);
        SDL_RenderFillRect(renderer, &rect);
        SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
    }

    bool isClicked(int mouseX, int mouseY) {
        return (mouseX >= rect.x && mouseX <= rect.x + rect.w &&
                mouseY >= rect.y && mouseY <= rect.y + rect.h);
    }
};

// کلاس مدیریت رابط کاربری
class AppManager {
private:
    bool isRunning;
    AppState currentState;
    std::vector<RecentProject> recentProjects;
    TTF_Font* mainFont;

    Button* btnNewProject;
    Button* btnA4;
    Button* btnA3;
    Button* btnCustom;

    SDL_Texture* recentProjectsTitle;
    SDL_Rect titleRect;
    std::vector<SDL_Texture*> recentTextTextures;
    std::vector<SDL_Rect> recentTextRects;

    // تابع هوشمند بارگذاری فونت با پشتیبانی از فونت‌های سیستم‌عامل
    TTF_Font* loadFont(int fontSize) {
        TTF_Font* font = TTF_OpenFont("font.ttf", fontSize);
        if (!font) {
            font = TTF_OpenFont("C:\\Windows\\Fonts\\arial.ttf", fontSize);
        }
        if (!font) {
            font = TTF_OpenFont("C:\\Windows\\Fonts\\tahoma.ttf", fontSize);
        }
        return font;
    }

public:
    AppManager(SDL_Renderer* renderer) : isRunning(true), currentState(AppState::MAIN_MENU) {
        // بارگذاری فونت به صورت هوشمند
        mainFont = loadFont(24);
        if (!mainFont) {
            std::cerr << "Failed to load font! Error: " << TTF_GetError() << "\n";
            exit(1);
        }

        SDL_Color white = {255, 255, 255, 255};
        SDL_Color btnColor = {52, 152, 219, 255};   // آبی
        SDL_Color sizeColor = {155, 89, 182, 255};  // بنفش

        // ساخت دکمه‌ها
        btnNewProject = new Button(50, 100, 300, 60, btnColor, white, "Create New Project", mainFont, renderer);

        btnA4 = new Button(250, 150, 300, 50, sizeColor, white, "A4 Size (210x297)", mainFont, renderer);
        btnA3 = new Button(250, 220, 300, 50, sizeColor, white, "A3 Size (297x420)", mainFont, renderer);
        btnCustom = new Button(250, 290, 300, 50, {241, 196, 15, 255}, white, "Custom Size", mainFont, renderer);

        // فهرست 5 پروژه اخیر
        recentProjects = {
                {"Transmission_Line_Sim", "1405/04/22"},
                {"Tesla_Motor_Control", "1405/04/20"},
                {"Sarcheshmeh_Power_Grid", "1405/04/18"},
                {"Wireless_Power_Transfer", "1405/04/15"},
                {"ML_Power_Optimization", "1405/04/10"}
        };

        // رندر کردن لیست پروژه‌های اخیر
        SDL_Surface* titleSurf = TTF_RenderText_Blended(mainFont, "Recent Projects:", white);
        recentProjectsTitle = SDL_CreateTextureFromSurface(renderer, titleSurf);
        titleRect = {450, 100, titleSurf->w, titleSurf->h};
        SDL_FreeSurface(titleSurf);

        TTF_Font* smallFont = loadFont(18);
        int startY = 160;
        for (const auto& proj : recentProjects) {
            std::string line = "- " + proj.name + " (" + proj.lastModified + ")";
            SDL_Surface* surf = TTF_RenderText_Blended(smallFont, line.c_str(), {200, 200, 200, 255});
            recentTextTextures.push_back(SDL_CreateTextureFromSurface(renderer, surf));
            recentTextRects.push_back({450, startY, surf->w, surf->h});
            startY += 40;
            SDL_FreeSurface(surf);
        }
        TTF_CloseFont(smallFont);
    }

    ~AppManager() {
        delete btnNewProject; delete btnA4; delete btnA3; delete btnCustom;
        SDL_DestroyTexture(recentProjectsTitle);
        for (auto tex : recentTextTextures) SDL_DestroyTexture(tex);
        if (mainFont) TTF_CloseFont(mainFont);
    }

    void handleEvents() {
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                isRunning = false;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = e.button.x;
                int my = e.button.y;

                if (currentState == AppState::MAIN_MENU) {
                    if (btnNewProject->isClicked(mx, my)) {
                        currentState = AppState::NEW_PROJECT_DIALOG;
                    }
                }
                else if (currentState == AppState::NEW_PROJECT_DIALOG) {
                    if (btnA4->isClicked(mx, my) || btnA3->isClicked(mx, my) || btnCustom->isClicked(mx, my)) {
                        currentState = AppState::WORKSPACE;
                    }
                }
            }
        }
    }

    void render(SDL_Renderer* renderer) {
        SDL_SetRenderDrawColor(renderer, 40, 44, 52, 255); // پس‌زمینه تیره
        SDL_RenderClear(renderer);

        if (currentState == AppState::MAIN_MENU) {
            btnNewProject->draw(renderer);

            // رسم لیست پروژه‌های اخیر
            SDL_RenderCopy(renderer, recentProjectsTitle, nullptr, &titleRect);
            for (size_t i = 0; i < recentTextTextures.size(); ++i) {
                SDL_RenderCopy(renderer, recentTextTextures[i], nullptr, &recentTextRects[i]);
            }
        }
        else if (currentState == AppState::NEW_PROJECT_DIALOG) {
            btnA4->draw(renderer);
            btnA3->draw(renderer);
            btnCustom->draw(renderer);
        }
        else if (currentState == AppState::WORKSPACE) {
            SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255); // رنگ روشن برای بوم طراحی
            SDL_RenderClear(renderer);
        }

        SDL_RenderPresent(renderer);
    }

    bool getIsRunning() const { return isRunning; }
};

#undef main
int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init(); // راه‌اندازی کتابخانه فونت

    SDL_Window* window = SDL_CreateWindow("Proteus Simulator - Menu",
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
