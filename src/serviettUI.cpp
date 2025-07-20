#include "serviettUI.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <fstream>
#include <string>
#include <vector>
#include <functional>

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;
static TTF_Font* font = nullptr;
static SDL_Texture* currentTarget = nullptr;
static SDL_Texture* nextTarget = nullptr;
static std::vector<float> springValues;
static constexpr int WIDTH = 400;
static constexpr int HEIGHT = 600;
static constexpr int SCALE = 2;
static constexpr int FPS = 60;
static constexpr int FRAME_DELAY = 1000 / FPS;
static constexpr int SPACING = 10 * SCALE;
static constexpr float PRESSED_ALPHA = 0.2f;
static constexpr float NORMAL_ALPHA = 1.0f;
static constexpr float OVERLAY_MAX_ALPHA = 0.6f;
static constexpr float ANIM_DURATION = 350.0f;

enum class DescType { Text, Button, Toggle };
struct Descriptor { DescType type; std::string label; std::function<void()> cb; bool* toggleState = nullptr; };
struct State { bool pressed = false; bool animating = false; Uint32 animStart = 0; float alpha = NORMAL_ALPHA; bool togAnimating = false; Uint32 togStart = 0; float togPos = 0.0f; };

static std::vector<Descriptor> curDesc, nxtDesc;
static std::vector<State> curStates, nxtStates;
static std::function<void()> curViewFunc, nxtViewFunc;
static bool animatingOverlay = false;
static Uint32 overlayStart = 0;

static std::vector<float> loadSpring(const char* path) {
    std::ifstream f(path);
    std::string all, line;
    while (std::getline(f, line)) all += line;
    std::vector<float> v;
    for (size_t i = 0; i < all.size();) {
        if ((all[i] >= '0' && all[i] <= '9') || all[i]=='-' || all[i]=='.') {
            size_t j = i;
            while (j < all.size() && ((all[j] >= '0' && all[j] <= '9') || all[j]=='-' || all[j]=='.')) j++;
            v.push_back(std::stof(all.substr(i, j - i)));
            i = j;
        } else i++;
    }
    return v;
}

void Text(const std::string& text) { curDesc.push_back({DescType::Text, text, {}, nullptr}); }
void Button(const std::string& label, const std::function<void()>& cb) { curDesc.push_back({DescType::Button, label, cb, nullptr}); }
void Toggle(const std::string& label, bool& state) { curDesc.push_back({DescType::Toggle, label, {}, &state}); }

void NewView(const std::function<void()>& viewFunc) {
    if (!animatingOverlay) {
        nxtViewFunc = viewFunc;
        nxtDesc.clear();
        nxtViewFunc();
        if (nxtStates.size() != nxtDesc.size()) nxtStates.assign(nxtDesc.size(), State());
        SDL_SetRenderTarget(renderer, nextTarget);
        SDL_SetRenderDrawColor(renderer, 255,255,255,255);
        SDL_RenderClear(renderer);
        int m = nxtDesc.size();
        std::vector<SDL_Texture*> tex(m);
        std::vector<SDL_Rect> rect(m);
        int totalH = 0;
        for (int i = 0; i < m; i++) {
            int w,h;
            TTF_SizeUTF8(font, nxtDesc[i].label.c_str(), &w, &h);
            totalH += h + (i?SPACING:0);
        }
        int y = (HEIGHT * SCALE - totalH) / 2;
        for (int i = 0; i < m; i++) {
            int w,h;
            TTF_SizeUTF8(font, nxtDesc[i].label.c_str(), &w, &h);
            rect[i] = {(WIDTH * SCALE - w) / 2, y, w, h};
            y += h + SPACING;
            SDL_Color col = (nxtDesc[i].type == DescType::Button ? SDL_Color{0,102,255,255} : SDL_Color{0,0,0,255});
            SDL_Surface* surf = TTF_RenderUTF8_Blended(font, nxtDesc[i].label.c_str(), col);
            tex[i] = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_FreeSurface(surf);
        }
        for (int i = 0; i < m; i++) {
            SDL_SetTextureAlphaMod(tex[i], Uint8(nxtStates[i].alpha * 255));
            SDL_RenderCopy(renderer, tex[i], nullptr, &rect[i]);
            SDL_DestroyTexture(tex[i]);
        }
        SDL_SetRenderTarget(renderer, nullptr);
        std::swap(currentTarget, nextTarget);
        curViewFunc = nxtViewFunc;
        curStates = nxtStates;
        animatingOverlay = true;
        overlayStart = SDL_GetTicks();
    }
}

void View(const std::function<void()>& viewFunc) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    springValues = loadSpring("./Resources/Spring.json");
    window = SDL_CreateWindow("serviettUI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    font = TTF_OpenFont("./Resources/Inter.ttf", 18 * SCALE);
    currentTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WIDTH * SCALE, HEIGHT * SCALE);
    nextTarget    = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WIDTH * SCALE, HEIGHT * SCALE);
    SDL_SetTextureBlendMode(currentTarget, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(nextTarget,    SDL_BLENDMODE_BLEND);
    curViewFunc = viewFunc;
    bool running = true;
    SDL_Event e;
    bool mouseDown = false, mouseUp = false;
    int downX=0, downY=0, upX=0, upY=0;
    curDesc.clear();
    curViewFunc();
    if (curStates.size() != curDesc.size()) curStates.assign(curDesc.size(), State());
    while (running) {
        Uint32 start = SDL_GetTicks();
        mouseDown = mouseUp = false;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_MOUSEBUTTONDOWN) { mouseDown = true; SDL_GetMouseState(&downX,&downY); downX *= SCALE; downY *= SCALE; }
            if (e.type == SDL_MOUSEBUTTONUP)   { mouseUp   = true; SDL_GetMouseState(&upX,&upY);   upX   *= SCALE; upY   *= SCALE; }
        }
        curDesc.clear();
        curViewFunc();
        if (curStates.size() != curDesc.size()) curStates.assign(curDesc.size(), State());
        SDL_SetRenderTarget(renderer, currentTarget);
        SDL_SetRenderDrawColor(renderer,255,255,255,255);
        SDL_RenderClear(renderer);
        int n = curDesc.size();
        std::vector<SDL_Texture*> tex(n);
        std::vector<SDL_Rect> rect(n);
        int totalH = 0;
        for (int i = 0; i < n; i++) {
            int w,h;
            TTF_SizeUTF8(font, curDesc[i].label.c_str(), &w, &h);
            totalH += h + SPACING;
        }
        int y = (HEIGHT * SCALE - totalH) / 2;
        for (int i = 0; i < n; i++) {
            int w,h;
            TTF_SizeUTF8(font, curDesc[i].label.c_str(), &w, &h);
            rect[i] = {(WIDTH * SCALE - w) / 2, y, w, h};
            y += h + SPACING;
            if (curDesc[i].type != DescType::Toggle) {
                SDL_Color col = (curDesc[i].type == DescType::Button ? SDL_Color{0,102,255,255} : SDL_Color{0,0,0,255});
                SDL_Surface* surf = TTF_RenderUTF8_Blended(font, curDesc[i].label.c_str(), col);
                tex[i] = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_FreeSurface(surf);
            }
        }
        for (int i = 0; i < n; i++) {
            State& st = curStates[i];
            if (curDesc[i].type == DescType::Button) {
                bool d = mouseDown && downX >= rect[i].x && downX <= rect[i].x + rect[i].w && downY >= rect[i].y && downY <= rect[i].y + rect[i].h;
                bool u = mouseUp   && upX   >= rect[i].x && upX   <= rect[i].x + rect[i].w && upY   >= rect[i].y && upY   <= rect[i].y + rect[i].h;
                if (d) { curDesc[i].cb(); st.pressed = true; st.animating = false; st.alpha = PRESSED_ALPHA; }
                if (mouseUp && st.pressed) { st.pressed = false; if (u) { st.animating = true; st.animStart = SDL_GetTicks(); } else st.alpha = NORMAL_ALPHA; }
                if (st.animating) {
                    float dt = float(SDL_GetTicks() - st.animStart) / ANIM_DURATION;
                    if (dt >= 1) { st.animating = false; st.alpha = NORMAL_ALPHA; }
                    else { float v = springValues[size_t(dt * (springValues.size() - 1))]; st.alpha = PRESSED_ALPHA + (NORMAL_ALPHA - PRESSED_ALPHA) * v; }
                }
                SDL_SetTextureAlphaMod(tex[i], Uint8(st.alpha * 255));
                SDL_RenderCopy(renderer, tex[i], nullptr, &rect[i]);
                SDL_DestroyTexture(tex[i]);
            }
            else if (curDesc[i].type == DescType::Text) {
                SDL_RenderCopy(renderer, tex[i], nullptr, &rect[i]);
                SDL_DestroyTexture(tex[i]);
            }
            else if (curDesc[i].type == DescType::Toggle) {
                int w,h;
                TTF_SizeUTF8(font, curDesc[i].label.c_str(), &w, &h);
                int ty = rect[i].y;
                int labelX = 5 * SCALE;
                SDL_Rect labelRect = { labelX, ty + (68 - h) / 2, w, h };
                SDL_Surface* surf = TTF_RenderUTF8_Blended(font, curDesc[i].label.c_str(), SDL_Color{0,0,0,255});
                SDL_Texture* labelTex = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_FreeSurface(surf);
                SDL_RenderCopy(renderer, labelTex, nullptr, &labelRect);
                SDL_DestroyTexture(labelTex);

                int tx = WIDTH * SCALE - 5 * SCALE - 132;
                int ty0 = ty;
                bool& s = *curDesc[i].toggleState;
                const int toggW = 132;
                const int toggH = 68;
                int innerPad = 5 * SCALE;
                int circleD = toggH - 2 * innerPad;
                int slideRange = toggW - 2 * innerPad - circleD;
                if (!st.togAnimating) st.togPos = s ? 1.0f : 0.0f;
                if (mouseUp && downX >= tx && downX <= tx + toggW && downY >= ty0 && downY <= ty0 + toggH) {
                    s = !s;
                    st.togAnimating = true;
                    st.togStart = SDL_GetTicks();
                }
                float vPos;
                if (st.togAnimating) {
                    float dt = float(SDL_GetTicks() - st.togStart) / ANIM_DURATION;
                    if (dt >= 1) { st.togAnimating = false; st.togPos = s ? 1.0f : 0.0f; }
                    else {
                        float r = springValues[size_t(dt * (springValues.size() - 1))];
                        st.togPos = s ? r : (1 - r);
                    }
                }
                vPos = st.togPos;
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                Uint8 r0=0xe9, g0=0xe9, b0=0xeb;
                Uint8 r1=0x69, g1=0xce, b1=0x67;
                Uint8 rc = Uint8(r0 + (r1 - r0) * vPos);
                Uint8 gc = Uint8(g0 + (g1 - g0) * vPos);
                Uint8 bc = Uint8(b0 + (b1 - b0) * vPos);
                roundedBoxRGBA(renderer, tx, ty0, tx + toggW - 1, ty0 + toggH - 1, toggH / 2, rc, gc, bc, 255);
                int cx = tx + innerPad + int(slideRange * vPos);
                int cy = ty0 + innerPad;
                filledCircleRGBA(renderer, cx + circleD/2, cy + circleD/2, circleD/2, 0xff, 0xff, 0xff, 255);
            }
        }

        SDL_SetRenderTarget(renderer, nullptr);
        SDL_SetRenderDrawColor(renderer,255,255,255,255);
        SDL_RenderClear(renderer);

        if (animatingOverlay) {
            float dt = float(SDL_GetTicks() - overlayStart) / ANIM_DURATION;
            float t = dt > 1 ? 1 : dt;
            float v2 = springValues[size_t(t * (springValues.size() - 1))];
            if (dt >= 1) animatingOverlay = false;
            int offOld = int(-0.5f * WIDTH * SCALE * v2);
            int offNew = int(WIDTH * SCALE * (1 - v2));
            SDL_Rect dstOld = {offOld,0,WIDTH*SCALE,HEIGHT*SCALE};
            SDL_RenderCopy(renderer, nextTarget,nullptr,&dstOld);
            SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer,0,0,0,Uint8(OVERLAY_MAX_ALPHA*v2*255));
            SDL_RenderFillRect(renderer,&dstOld);
            SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_NONE);
            SDL_Rect dstNew = {offNew,0,WIDTH*SCALE,HEIGHT*SCALE};
            SDL_RenderCopy(renderer,currentTarget,nullptr,&dstNew);
        } else {
            SDL_RenderCopy(renderer,currentTarget,nullptr,nullptr);
        }

        SDL_RenderPresent(renderer);
        Uint32 elapsed = SDL_GetTicks() - start;
        if (elapsed < FRAME_DELAY) SDL_Delay(FRAME_DELAY - elapsed);
    }

    SDL_DestroyTexture(nextTarget);
    SDL_DestroyTexture(currentTarget);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}
