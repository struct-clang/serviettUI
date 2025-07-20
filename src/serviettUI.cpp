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
static SDL_Texture*  currentTarget = nullptr;
static SDL_Texture*  nextTarget    = nullptr;
static std::vector<float> springValues;
static constexpr int WIDTH       = 400;
static constexpr int HEIGHT      = 600;
static constexpr int SCALE       = 2;
static constexpr int FPS         = 60;
static constexpr int FRAME_DELAY = 1000 / FPS;
static constexpr int SPACING     = 10 * SCALE;
static constexpr float PRESSED_ALPHA = 0.2f;
static constexpr float NORMAL_ALPHA  = 1.0f;
static constexpr float OVERLAY_MAX_ALPHA = 0.7f;
static constexpr float ANIM_DURATION = 350.0f;

struct Descriptor { bool isButton; std::string label; std::function<void()> cb; };
struct State { bool pressed = false; bool animating = false; Uint32 animStart = 0; float alpha = NORMAL_ALPHA; };

static std::vector<Descriptor> curDesc, nxtDesc;
static std::vector<State> curStates, nxtStates;
static std::function<void()> curViewFunc;
static std::function<void()> nxtViewFunc;
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

void Text(const std::string& text) {
    curDesc.push_back({false, text, {}});
}

void Button(const std::string& label, const std::function<void()>& cb) {
    curDesc.push_back({true, label, cb});
}

void NewView(const std::function<void()>& viewFunc) {
    if (!animatingOverlay) {
        nxtViewFunc = viewFunc;
        // prepare next content immediately
        nxtDesc.clear();
        nxtViewFunc();
        if (nxtStates.size() != nxtDesc.size()) nxtStates.assign(nxtDesc.size(), State());
        SDL_SetRenderTarget(renderer, nextTarget);
        SDL_SetRenderDrawColor(renderer, 255,255,255,255);
        SDL_RenderClear(renderer);
        int m = nxtDesc.size();
        std::vector<SDL_Texture*> tex2(m);
        std::vector<SDL_Rect> rect2(m);
        int totalH2=0;
        for(int i=0;i<m;i++){int w,h; TTF_SizeUTF8(font,nxtDesc[i].label.c_str(),&w,&h); totalH2+=h+(i?SPACING:0);} 
        int y2=(HEIGHT*SCALE-totalH2)/2;
        for(int i=0;i<m;i++){
            int w,h; TTF_SizeUTF8(font,nxtDesc[i].label.c_str(),&w,&h);
            rect2[i] = {(WIDTH*SCALE-w)/2, y2, w, h}; y2+=h+SPACING;
            SDL_Color col = nxtDesc[i].isButton ? SDL_Color{0,102,255,255} : SDL_Color{0,0,0,255};
            SDL_Surface* surf = TTF_RenderUTF8_Blended(font, nxtDesc[i].label.c_str(), col);
            tex2[i] = SDL_CreateTextureFromSurface(renderer, surf); SDL_FreeSurface(surf);
        }
        for(int i=0;i<m;i++){
            SDL_SetTextureAlphaMod(tex2[i], Uint8(nxtStates[i].alpha * 255));
            SDL_RenderCopy(renderer, tex2[i], nullptr, &rect2[i]);
            SDL_DestroyTexture(tex2[i]);
        }
        SDL_SetRenderTarget(renderer, nullptr);
        animatingOverlay = true;
        overlayStart = SDL_GetTicks();
    }
}

void View(const std::function<void()>& viewFunc) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
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
    bool mouseDown=false, mouseUp=false;
    int downX=0, downY=0, upX=0, upY=0;

    // initial render of current view
    curDesc.clear(); curViewFunc();
    if (curStates.size() != curDesc.size()) curStates.assign(curDesc.size(), State());
    SDL_SetRenderTarget(renderer, currentTarget);
    SDL_SetRenderDrawColor(renderer,255,255,255,255);
    SDL_RenderClear(renderer);
    int n = curDesc.size();
    std::vector<SDL_Texture*> tex(n);
    std::vector<SDL_Rect> rect(n);
    int totalH=0;
    for(int i=0;i<n;i++){int w,h; TTF_SizeUTF8(font,curDesc[i].label.c_str(),&w,&h); totalH+=h+(i?SPACING:0);} 
    int y=(HEIGHT*SCALE-totalH)/2;
    for(int i=0;i<n;i++){
        int w,h; TTF_SizeUTF8(font,curDesc[i].label.c_str(),&w,&h);
        rect[i] = {(WIDTH*SCALE-w)/2, y, w, h}; y+=h+SPACING;
        SDL_Color col = curDesc[i].isButton ? SDL_Color{0,102,255,255} : SDL_Color{0,0,0,255};
        SDL_Surface* surf = TTF_RenderUTF8_Blended(font, curDesc[i].label.c_str(), col);
        tex[i] = SDL_CreateTextureFromSurface(renderer, surf); SDL_FreeSurface(surf);
    }
    for(int i=0;i<n;i++){
        State& st = curStates[i];
        if(curDesc[i].isButton) SDL_SetTextureAlphaMod(tex[i], Uint8(st.alpha*255));
        SDL_RenderCopy(renderer, tex[i], nullptr, &rect[i]);
        SDL_DestroyTexture(tex[i]);
    }
    SDL_SetRenderTarget(renderer, nullptr);

    while (running) {
        Uint32 start = SDL_GetTicks();
        mouseDown = mouseUp = false;
        while (SDL_PollEvent(&e)) {
            if(e.type == SDL_QUIT) running = false;
            if(e.type == SDL_MOUSEBUTTONDOWN) { mouseDown=true; SDL_GetMouseState(&downX,&downY); downX*=SCALE; downY*=SCALE; }
            if(e.type == SDL_MOUSEBUTTONUP)   { mouseUp=true;   SDL_GetMouseState(&upX,&upY);   upX*=SCALE;   upY*=SCALE;   }
        }

        // re-render current to texture
        curDesc.clear(); curViewFunc();
        if (curStates.size() != curDesc.size()) curStates.assign(curDesc.size(), State());
        SDL_SetRenderTarget(renderer, currentTarget);
        SDL_SetRenderDrawColor(renderer,255,255,255,255);
        SDL_RenderClear(renderer);
        totalH=0; for(int i=0;i<n;i++){int w,h; TTF_SizeUTF8(font,curDesc[i].label.c_str(),&w,&h); totalH+=h+(i?SPACING:0);} y=(HEIGHT*SCALE-totalH)/2;
        for(int i=0;i<n;i++){
            int w,h; TTF_SizeUTF8(font,curDesc[i].label.c_str(),&w,&h);
            rect[i] = {(WIDTH*SCALE-w)/2, y, w, h}; y+=h+SPACING;
            SDL_Color col = curDesc[i].isButton ? SDL_Color{0,102,255,255} : SDL_Color{0,0,0,255};
            SDL_Surface* surf = TTF_RenderUTF8_Blended(font, curDesc[i].label.c_str(), col);
            tex[i] = SDL_CreateTextureFromSurface(renderer, surf); SDL_FreeSurface(surf);
        }
        for(int i=0;i<n;i++){
            State& st = curStates[i];
            if(curDesc[i].isButton) {
                bool d = mouseDown && downX>=rect[i].x && downX<=rect[i].x+rect[i].w && downY>=rect[i].y && downY<=rect[i].y+rect[i].h;
                bool u = mouseUp   && upX>=rect[i].x   && upX<=rect[i].x+rect[i].w   && upY>=rect[i].y   && upY<=rect[i].y+rect[i].h;
                if(d){ curDesc[i].cb(); st.pressed=true; st.animating=false; st.alpha=PRESSED_ALPHA; }
                if(mouseUp && st.pressed){ st.pressed=false; if(u){ st.animating=true; st.animStart=SDL_GetTicks(); } else st.alpha=NORMAL_ALPHA; }
                if(st.animating){ float dt=(SDL_GetTicks()-st.animStart)/ANIM_DURATION; if(dt>=1){ st.animating=false; st.alpha=NORMAL_ALPHA; } else { float v=springValues[size_t(dt*(springValues.size()-1))]; st.alpha=PRESSED_ALPHA + (NORMAL_ALPHA-PRESSED_ALPHA)*v; }}
                SDL_SetTextureAlphaMod(tex[i], Uint8(st.alpha*255));
            }
            SDL_RenderCopy(renderer, tex[i], nullptr, &rect[i]);
            SDL_DestroyTexture(tex[i]);
        }
        SDL_SetRenderTarget(renderer, nullptr);

        // composite to screen
        SDL_SetRenderDrawColor(renderer,255,255,255,255);
        SDL_RenderClear(renderer);
        if(animatingOverlay) {
            float dt = float(SDL_GetTicks()-overlayStart)/ANIM_DURATION;
            if(dt>=1){ animatingOverlay=false; curViewFunc = nxtViewFunc; }
            float t = dt>1?1:dt;
            size_t idx = size_t(t*(springValues.size()-1));
            float v = springValues[idx];
            int offCur = int(-0.5f*WIDTH*SCALE*v);
            int offNext = int(WIDTH*SCALE*(1-v));
            float alpha = OVERLAY_MAX_ALPHA * v;
            SDL_Rect dstCur = {offCur,0,WIDTH*SCALE,HEIGHT*SCALE};
            SDL_SetTextureAlphaMod(currentTarget, Uint8(alpha*255));
            SDL_RenderCopy(renderer, currentTarget, nullptr, &dstCur);
            SDL_SetTextureAlphaMod(currentTarget, 255);
            SDL_Rect dstNext = {offNext,0,WIDTH*SCALE,HEIGHT*SCALE};
            SDL_RenderCopy(renderer, nextTarget, nullptr, &dstNext);
        } else {
            SDL_RenderCopy(renderer, currentTarget, nullptr, nullptr);
        }
        SDL_RenderPresent(renderer);

        Uint32 elapsed = SDL_GetTicks() - start;
        if(elapsed < FRAME_DELAY) SDL_Delay(FRAME_DELAY - elapsed);
    }

    SDL_DestroyTexture(nextTarget);
    SDL_DestroyTexture(currentTarget);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
}
