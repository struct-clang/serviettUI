#include "serviettUI.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <fstream>
#include <vector>
#include <string>
#include <functional>

static SDL_Window*   window      = nullptr;
static SDL_Renderer* renderer    = nullptr;
static TTF_Font*     font        = nullptr;
static SDL_Texture*  target      = nullptr;
static std::vector<float> springValues;
static constexpr int WIDTH       = 400;
static constexpr int HEIGHT      = 600;
static constexpr int SCALE       = 2;
static constexpr int FPS         = 60;
static constexpr int FRAME_DELAY = 1000 / FPS;
static constexpr int SPACING     = 10 * SCALE;
static constexpr float PRESSED_ALPHA = 0.2f;
static constexpr float NORMAL_ALPHA  = 1.0f;
static constexpr float ANIM_DURATION = 350.0f;

struct Descriptor {
    bool isButton;
    std::string label;
    std::function<void()> cb;
};
struct State {
    bool pressed = false;
    bool animating = false;
    Uint32 animStart = 0;
    float alpha = NORMAL_ALPHA;
};

static std::vector<Descriptor> descriptors;
static std::vector<State> states;

static std::vector<float> loadSpring(const char* path){
    std::ifstream f(path);
    std::string all, line;
    while(std::getline(f, line)) all += line;
    std::vector<float> v;
    for(size_t i = 0; i < all.size(); ){
        if((all[i] >= '0' && all[i] <= '9') || all[i]=='-' || all[i]=='.'){
            size_t j = i;
            while(j < all.size() && ((all[j] >= '0' && all[j] <= '9') || all[j]=='-' || all[j]=='.')) j++;
            v.push_back(std::stof(all.substr(i, j - i)));
            i = j;
        } else i++;
    }
    return v;
}

void Text(const std::string& text){
    descriptors.push_back({false, text, {}});
}

void Button(const std::string& label, const std::function<void()>& cb){
    descriptors.push_back({true, label, cb});
}

void View(const std::function<void()>& viewFunc){
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
    springValues = loadSpring("./Resources/Spring.json");
    window = SDL_CreateWindow("serviettUI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    font = TTF_OpenFont("./Resources/Inter.ttf", 18 * SCALE);
    target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WIDTH * SCALE, HEIGHT * SCALE);

    bool running = true;
    SDL_Event e;
    bool mouseDown = false, mouseUp = false;
    int downX = 0, downY = 0, upX = 0, upY = 0;

    while(running){
        Uint32 start = SDL_GetTicks();
        mouseDown = mouseUp = false;
        while(SDL_PollEvent(&e)){
            if(e.type == SDL_QUIT) running = false;
            if(e.type == SDL_MOUSEBUTTONDOWN){
                mouseDown = true;
                int mx, my; SDL_GetMouseState(&mx, &my);
                downX = mx * SCALE; downY = my * SCALE;
            }
            if(e.type == SDL_MOUSEBUTTONUP){
                mouseUp = true;
                int mx, my; SDL_GetMouseState(&mx, &my);
                upX = mx * SCALE; upY = my * SCALE;
            }
        }

        descriptors.clear();
        viewFunc();
        if(states.size() != descriptors.size()){
            states.assign(descriptors.size(), State());
        }

        SDL_SetRenderTarget(renderer, target);
        SDL_SetRenderDrawColor(renderer, 255,255,255,255);
        SDL_RenderClear(renderer);

        int n = descriptors.size();
        std::vector<SDL_Texture*> tex(n);
        std::vector<SDL_Rect> rect(n);
        int totalH = 0;
        for(int i = 0; i < n; ++i){
            int w, h;
            TTF_SizeUTF8(font, descriptors[i].label.c_str(), &w, &h);
            totalH += h + (i>0 ? SPACING : 0);
        }
        int y = (HEIGHT * SCALE - totalH) / 2;
        for(int i = 0; i < n; ++i){
            int w, h;
            TTF_SizeUTF8(font, descriptors[i].label.c_str(), &w, &h);
            rect[i] = { (WIDTH * SCALE - w) / 2, y, w, h };
            y += h + SPACING;
            SDL_Color col = descriptors[i].isButton
                ? SDL_Color{Uint8(0), Uint8(102), Uint8(255), Uint8(255)}
                : SDL_Color{Uint8(0), Uint8(0), Uint8(0), Uint8(255)};
            SDL_Surface* surf = TTF_RenderUTF8_Blended(font, descriptors[i].label.c_str(), col);
            tex[i] = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_FreeSurface(surf);
        }

        for(int i = 0; i < n; ++i){
            State& st = states[i];
            if(descriptors[i].isButton){
                bool insideDown = mouseDown
                    && downX >= rect[i].x && downX <= rect[i].x + rect[i].w
                    && downY >= rect[i].y && downY <= rect[i].y + rect[i].h;
                bool insideUp = mouseUp
                    && upX >= rect[i].x && upX <= rect[i].x + rect[i].w
                    && upY >= rect[i].y && upY <= rect[i].y + rect[i].h;

                if(insideDown){
                    descriptors[i].cb();
                    st.pressed = true;
                    st.animating = false;
                    st.alpha = PRESSED_ALPHA;
                }
                if(mouseUp && st.pressed){
                    st.pressed = false;
                    if(insideUp){
                        st.animating = true;
                        st.animStart = SDL_GetTicks();
                    } else {
                        st.alpha = NORMAL_ALPHA;
                    }
                }
                if(st.animating){
                    float dt = float(SDL_GetTicks() - st.animStart) / ANIM_DURATION;
                    if(dt >= 1.0f){
                        st.animating = false;
                        st.alpha = NORMAL_ALPHA;
                    } else {
                        size_t idx = size_t(dt * (springValues.size() - 1));
                        float v = springValues[idx];
                        st.alpha = PRESSED_ALPHA + (NORMAL_ALPHA - PRESSED_ALPHA) * v;
                    }
                }
                SDL_SetTextureAlphaMod(tex[i], Uint8(st.alpha * 255));
            }
            SDL_RenderCopy(renderer, tex[i], nullptr, &rect[i]);
            SDL_DestroyTexture(tex[i]);
        }

        SDL_SetRenderTarget(renderer, nullptr);
        SDL_SetRenderDrawColor(renderer, 255,255,255,255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, target, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        Uint32 elapsed = SDL_GetTicks() - start;
        if(elapsed < FRAME_DELAY) SDL_Delay(FRAME_DELAY - elapsed);
    }

    SDL_DestroyTexture(target);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}
