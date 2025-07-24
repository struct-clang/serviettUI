#include "serviettUI.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL_image.h>
#include <fstream>
#include <string>
#include <vector>
#include <functional>

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;
static TTF_Font* font = nullptr;
static TTF_Font* titleFont = nullptr;
static SDL_Texture* currentTarget = nullptr;
static SDL_Texture* nextTarget = nullptr;
static std::vector<float> springValues;
static constexpr int WIDTH = 400;
static constexpr int HEIGHT = 600;
static constexpr int SCALE = 2;
static constexpr int FPS = 60;
static constexpr int FRAME_DELAY = 1000 / FPS;
static constexpr int SPACING = 10 * SCALE;
static constexpr int V_PADDING = 0;
static constexpr float PRESSED_ALPHA = 0.2f;
static constexpr float NORMAL_ALPHA = 1.0f;
static constexpr float OVERLAY_MAX_ALPHA = 0.6f;
static constexpr float ANIM_DURATION = 350.0f;
static constexpr int TF_HEIGHT = 32 * SCALE;
static constexpr int TF_PADDING = 10 * SCALE;
static constexpr int TF_RADIUS = 8 * SCALE;
static constexpr Uint32 CURSOR_BLINK_INTERVAL = 500;

enum class DescType { Text, Title, Button, Toggle, TextField, HStack, Image };
struct Descriptor {
    DescType type;
    std::string label;
    std::function<void()> cb;
    bool* toggleState;
    std::string* textState;
    int imgW;
    int imgH;
};
struct State {
    bool pressed = false;
    bool animating = false;
    Uint32 animStart = 0;
    float alpha = NORMAL_ALPHA;
    bool togPressed = false;
    bool togAnimating = false;
    Uint32 togStart = 0;
    float togPos = 0.0f;
    bool togPending = false;
    bool togTarget = false;
    Uint32 lastBlink = 0;
    bool showCursor = true;
    bool tfFocused = false;
};

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

void Text(const std::string& text) {
    curDesc.push_back({DescType::Text, text, {}, nullptr, nullptr, 0, 0});
}
void Title(const std::string& text) {
    curDesc.push_back({DescType::Title, text, {}, nullptr, nullptr, 0, 0});
}
void Button(const std::string& label, const std::function<void()>& cb) {
    curDesc.push_back({DescType::Button, label, cb, nullptr, nullptr, 0, 0});
}
void Toggle(const std::string& label, bool& state) {
    curDesc.push_back({DescType::Toggle, label, {}, &state, nullptr, 0, 0});
}
void TextField(const std::string& placeholder, std::string& state) {
    curDesc.push_back({DescType::TextField, placeholder, {}, nullptr, &state, 0, 0});
}
void HStack(const std::function<void()>& cb) {
    curDesc.push_back({DescType::HStack, {}, cb, nullptr, nullptr, 0, 0});
}
void Image(const std::string& path, int w, int h) {
    curDesc.push_back({DescType::Image, path, {}, nullptr, nullptr, w, h});
}

static void eraseLastUtf8Char(std::string& s) {
    if (s.empty()) return;
    size_t i = s.size() - 1;
    while (i > 0 && (s[i] & 0xC0) == 0x80) i--;
    s.erase(i);
}

void NewView(const std::function<void()>& viewFunc) {
    if (animatingOverlay) return;
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
    std::vector<std::vector<Descriptor>> hchildren(m);
    int totalH = 0;
    for (int i = 0; i < m; i++) {
        if (nxtDesc[i].type == DescType::Toggle) {
            totalH += 68 + 2 * V_PADDING + (i ? SPACING : 0);
        } else if (nxtDesc[i].type == DescType::TextField) {
            totalH += TF_HEIGHT + (i ? SPACING : 0);
        } else if (nxtDesc[i].type == DescType::HStack) {
            std::vector<Descriptor> tmp;
            auto save = curDesc; curDesc.clear();
            nxtDesc[i].cb();
            tmp = curDesc;
            curDesc = save;
            int maxh = 0;
            for (auto& d : tmp) {
                int w,h;
                if (d.type==DescType::Text||d.type==DescType::Button||d.type==DescType::Title)
                    TTF_SizeUTF8(d.type==DescType::Title?titleFont:font, d.label.c_str(), &w,&h),
                    maxh = std::max(maxh, h);
            }
            totalH += maxh + (i?SPACING:0);
        } else if (nxtDesc[i].type == DescType::Image) {
            totalH += nxtDesc[i].imgH + (i ? SPACING : 0);
        } else if (nxtDesc[i].type == DescType::Title) {
            int w, h;
            TTF_SizeUTF8(titleFont, nxtDesc[i].label.c_str(), &w, &h);
            totalH += h + (i ? SPACING : 0);
        } else {
            int w,h;
            TTF_SizeUTF8(font, nxtDesc[i].label.c_str(), &w, &h);
            totalH += h + (i ? SPACING : 0);
        }
    }
    int y = (HEIGHT * SCALE - totalH) / 2;
    for (int i = 0; i < m; i++) {
        if (nxtDesc[i].type == DescType::Toggle) {
            rect[i] = {0, y, WIDTH * SCALE, 68 + 2 * V_PADDING};
            y += 68 + 2 * V_PADDING + SPACING;
        } else if (nxtDesc[i].type == DescType::TextField) {
            rect[i] = {TF_PADDING, y, WIDTH * SCALE - 2 * TF_PADDING, TF_HEIGHT};
            y += TF_HEIGHT + SPACING;
        } else if (nxtDesc[i].type == DescType::HStack) {
            int height = 0;
            std::vector<Descriptor> tmp;
            auto save = curDesc; curDesc.clear();
            nxtDesc[i].cb();
            tmp = curDesc;
            curDesc = save;
            for (auto& d : tmp) {
                int w,h;
                if (d.type==DescType::Text||d.type==DescType::Button||d.type==DescType::Title)
                    TTF_SizeUTF8(d.type==DescType::Title?titleFont:font, d.label.c_str(), &w,&h),
                    height = std::max(height, h);
            }
            rect[i] = {0, y, WIDTH * SCALE, height};
            y += height + SPACING;
        } else if (nxtDesc[i].type == DescType::Image) {
            rect[i] = {(WIDTH * SCALE - nxtDesc[i].imgW) / 2, y, nxtDesc[i].imgW, nxtDesc[i].imgH};
            y += nxtDesc[i].imgH + SPACING;
        } else if (nxtDesc[i].type == DescType::Title) {
            int w,h;
            TTF_SizeUTF8(titleFont, nxtDesc[i].label.c_str(), &w, &h);
            rect[i] = {(WIDTH * SCALE - w) / 2, y, w, h};
            y += h + SPACING;
        } else {
            int w,h;
            TTF_SizeUTF8(font, nxtDesc[i].label.c_str(), &w, &h);
            rect[i] = {(WIDTH * SCALE - w) / 2, y, w, h};
            y += h + SPACING;
        }
    }
    for (int i = 0; i < m; i++) {
        State& st = nxtStates[i];
        if (nxtDesc[i].type == DescType::Button) {
            SDL_Color col = {0,102,255,255};
            SDL_Surface* surf = TTF_RenderUTF8_Blended(font, nxtDesc[i].label.c_str(), col);
            tex[i] = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_FreeSurface(surf);
            SDL_SetTextureAlphaMod(tex[i], Uint8(st.alpha * 255));
            SDL_RenderCopy(renderer, tex[i], nullptr, &rect[i]);
            SDL_DestroyTexture(tex[i]);
        } else if (nxtDesc[i].type == DescType::Text) {
            SDL_Color col = {0,0,0,255};
            SDL_Surface* surf = TTF_RenderUTF8_Blended(font, nxtDesc[i].label.c_str(), col);
            tex[i] = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_FreeSurface(surf);
            SDL_RenderCopy(renderer, tex[i], nullptr, &rect[i]);
            SDL_DestroyTexture(tex[i]);
        } else if (nxtDesc[i].type == DescType::Title) {
            SDL_Color col = {0,0,0,255};
            SDL_Surface* surf = TTF_RenderUTF8_Blended(titleFont, nxtDesc[i].label.c_str(), col);
            tex[i] = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_FreeSurface(surf);
            SDL_RenderCopy(renderer, tex[i], nullptr, &rect[i]);
            SDL_DestroyTexture(tex[i]);
        } else if (nxtDesc[i].type == DescType::Toggle) {
            int ty = rect[i].y;
            int w,h;
            TTF_SizeUTF8(font, nxtDesc[i].label.c_str(), &w, &h);
            SDL_Surface* surf = TTF_RenderUTF8_Blended(font, nxtDesc[i].label.c_str(), SDL_Color{0,0,0,255});
            SDL_Texture* labelTex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_FreeSurface(surf);
            SDL_Rect lr = {5 * SCALE, ty + V_PADDING + (68 - h) / 2, w, h};
            SDL_RenderCopy(renderer, labelTex, nullptr, &lr);
            SDL_DestroyTexture(labelTex);
            int tx = WIDTH * SCALE - 5 * SCALE - 132;
            int ty0 = ty + V_PADDING;
            bool& s = *nxtDesc[i].toggleState;
            int toggW = 132, toggH = 68, innerPad = 5 * SCALE;
            int circleD = toggH - 2 * innerPad;
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            Uint8 r0=0xe9,g0=0xe9,b0=0xeb;
            Uint8 r1=0x69,g1=0xce,b1=0x67;
            Uint8 rc = s?r1:r0, gc = s?g1:g0, bc = s?b1:b0;
            roundedBoxRGBA(renderer, tx, ty0, tx + toggW - 1, ty0 + toggH - 1, toggH/2, rc, gc, bc, 255);
            int cx = tx + innerPad + (s ? (toggW - 2*innerPad - circleD) : 0);
            int cy = ty0 + innerPad;
            filledCircleRGBA(renderer, cx + circleD/2, cy + circleD/2, circleD/2, 0xff,0xff,0xff,255);
        } else if (nxtDesc[i].type == DescType::TextField) {
            State& stf = nxtStates[i];
            roundedBoxRGBA(renderer, rect[i].x, rect[i].y, rect[i].x + rect[i].w, rect[i].y + rect[i].h, TF_RADIUS, 255,255,255,255);
            roundedRectangleRGBA(renderer, rect[i].x, rect[i].y, rect[i].x + rect[i].w, rect[i].y + rect[i].h, TF_RADIUS, 0x88,0x88,0x88,255);
            std::string* txt = nxtDesc[i].textState;
            bool empty = txt->empty();
            SDL_Color tcol = empty?SDL_Color{0x88,0x88,0x88,255}:SDL_Color{0,0,0,255};
            SDL_Surface* surf = TTF_RenderUTF8_Blended(font, (empty?nxtDesc[i].label:*txt).c_str(), tcol);
            SDL_Texture* ttx = SDL_CreateTextureFromSurface(renderer, surf);
            int th = surf->h, tw = surf->w;
            SDL_FreeSurface(surf);
            SDL_Rect tr = {rect[i].x + 5, rect[i].y + (TF_HEIGHT - th)/2, tw, th};
            SDL_RenderCopy(renderer, ttx, nullptr, &tr);
            SDL_DestroyTexture(ttx);
            Uint32 now = SDL_GetTicks();
            if (stf.tfFocused) {
                if (now - stf.lastBlink > CURSOR_BLINK_INTERVAL) {
                    stf.showCursor = !stf.showCursor;
                    stf.lastBlink = now;
                }
                if (stf.showCursor) {
                    int caretX = empty? rect[i].x + 5 : tr.x + tw;
                    int cy0 = rect[i].y + 4;
                    int cy1 = rect[i].y + TF_HEIGHT - 4;
                    SDL_RenderDrawLine(renderer, caretX, cy0, caretX, cy1);
                }
            }
        } else if (nxtDesc[i].type == DescType::HStack) {
            auto& tmp = hchildren[i];
            int count = tmp.size();
            if (!count) continue;
            int slotW = WIDTH * SCALE / count;
            for (int j = 0; j < count; j++) {
                auto& d = tmp[j];
                int x0 = j * slotW;
                int y0 = rect[i].y;
                if (d.type == DescType::Text || d.type == DescType::Button || d.type == DescType::Title) {
                    SDL_Color col = d.type==DescType::Button? SDL_Color{0,102,255,255} : SDL_Color{0,0,0,255};
                    TTF_Font* f = d.type==DescType::Title? titleFont : font;
                    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, d.label.c_str(), col);
                    SDL_Texture* txr = SDL_CreateTextureFromSurface(renderer, surf);
                    int w,h; SDL_QueryTexture(txr, nullptr, nullptr, &w,&h);
                    SDL_FreeSurface(surf);
                    SDL_Rect rr = {x0 + (slotW - w)/2, y0 + (rect[i].h - h)/2, w,h};
                    SDL_RenderCopy(renderer, txr, nullptr, &rr);
                    SDL_DestroyTexture(txr);
                }
            }
        } else if (nxtDesc[i].type == DescType::Image) {
            SDL_Surface* surf = IMG_Load(("./Resources/" + nxtDesc[i].label).c_str());
            SDL_Texture* imgTex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_FreeSurface(surf);
            SDL_RenderCopy(renderer, imgTex, nullptr, &rect[i]);
            SDL_DestroyTexture(imgTex);
        }
    }
    SDL_SetRenderTarget(renderer, nullptr);
    std::swap(currentTarget, nextTarget);
    curViewFunc = nxtViewFunc;
    curStates = nxtStates;
    animatingOverlay = true;
    overlayStart = SDL_GetTicks();
}

void View(const std::function<void()>& viewFunc) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);
    springValues = loadSpring("./Resources/Spring.json");
    window = SDL_CreateWindow("serviettUI", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, SDL_WINDOW_ALLOW_HIGHDPI);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    font = TTF_OpenFont("./Resources/Inter.ttf", 18 * SCALE);
    titleFont = TTF_OpenFont("./Resources/Inter.ttf", 36 * SCALE);
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
    SDL_StartTextInput();
    while (running) {
        Uint32 start = SDL_GetTicks();
        mouseDown = mouseUp = false;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                mouseDown = true;
                SDL_GetMouseState(&downX,&downY);
                downX *= SCALE; downY *= SCALE;
                int totalH = 0;
                for (auto& d : curDesc) {
                    if (d.type == DescType::Toggle) totalH += 68+2*V_PADDING+SPACING;
                    else if (d.type == DescType::TextField) totalH += TF_HEIGHT+SPACING;
                    else if (d.type == DescType::HStack) {
                        std::vector<Descriptor> tmp;
                        auto save=curDesc; curDesc.clear();
                        d.cb();
                        tmp=curDesc;
                        curDesc=save;
                        int hh=0;
                        for (auto& c:tmp){
                            int w,h;
                            if (c.type==DescType::Text||c.type==DescType::Button||c.type==DescType::Title)
                                TTF_SizeUTF8(c.type==DescType::Title?titleFont:font, c.label.c_str(), &w,&h),
                                hh=std::max(hh,h);
                        }
                        totalH += hh+SPACING;
                    } else if (d.type == DescType::Image) totalH += d.imgH + SPACING;
                    else if (d.type == DescType::Title) {
                        int w,h;
                        TTF_SizeUTF8(titleFont, d.label.c_str(), &w,&h);
                        totalH += h+SPACING;
                    } else {
                        int w,h;
                        TTF_SizeUTF8(font,d.label.c_str(),&w,&h);
                        totalH+=h+SPACING;
                    }
                }
                int yy = (HEIGHT*SCALE - totalH)/2;
                for (int i = 0; i < (int)curDesc.size(); i++) {
                    if (curDesc[i].type == DescType::TextField) {
                        SDL_Rect r = {TF_PADDING, yy, WIDTH*SCALE-2*TF_PADDING, TF_HEIGHT};
                        yy += TF_HEIGHT+SPACING;
                        if (downX>=r.x&&downX<=r.x+r.w&&downY>=r.y&&downY<=r.y+r.h) curStates[i].tfFocused=true;
                        else curStates[i].tfFocused=false;
                    } else if (curDesc[i].type == DescType::Toggle) yy += 68+2*V_PADDING+SPACING;
                    else if (curDesc[i].type == DescType::HStack) {
                        std::vector<Descriptor> tmp;
                        auto save=curDesc; curDesc.clear();
                        curDesc[i].cb();
                        tmp=curDesc;
                        curDesc=save;
                        int hh=0;
                        for (auto& c:tmp){
                            int w,h;
                            if (c.type==DescType::Text||c.type==DescType::Button||c.type==DescType::Title)
                                TTF_SizeUTF8(c.type==DescType::Title?titleFont:font, c.label.c_str(), &w,&h),
                                hh=std::max(hh,h);
                        }
                        yy+=hh+SPACING;
                    } else if (curDesc[i].type == DescType::Image) {
                        yy += curDesc[i].imgH + SPACING;
                    } else if (curDesc[i].type == DescType::Title) {
                        int w,h;
                        TTF_SizeUTF8(titleFont, curDesc[i].label.c_str(), &w,&h);
                        yy += h+SPACING;
                    } else {
                        int w,h;
                        TTF_SizeUTF8(font,curDesc[i].label.c_str(),&w,&h);
                        yy+=h+SPACING;
                    }
                }
            }
            if (e.type == SDL_MOUSEBUTTONUP) {
                mouseUp = true;
                SDL_GetMouseState(&upX,&upY);
                upX *= SCALE; upY *= SCALE;
            }
            if (e.type == SDL_TEXTINPUT) {
                for (int i = 0; i < (int)curDesc.size(); i++) {
                    if (curDesc[i].type == DescType::TextField && curStates[i].tfFocused) {
                        *curDesc[i].textState += e.text.text;
                    }
                }
            }
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_BACKSPACE) {
                for (int i = 0; i < (int)curDesc.size(); i++) {
                    if (curDesc[i].type == DescType::TextField && curStates[i].tfFocused) {
                        eraseLastUtf8Char(*curDesc[i].textState);
                    }
                }
            }
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
        std::vector<std::vector<Descriptor>> hchildren(n);
        int totalH = 0;
        for (int i = 0; i < n; i++) {
            if (curDesc[i].type == DescType::Toggle) totalH += 68+2*V_PADDING+(i?SPACING:0);
            else if (curDesc[i].type == DescType::TextField) totalH += TF_HEIGHT+(i?SPACING:0);
            else if (curDesc[i].type == DescType::HStack) {
                auto save=curDesc; curDesc.clear();
                curDesc[i].cb();
                hchildren[i]=curDesc;
                curDesc=save;
                int hh=0;
                for(auto&d:hchildren[i]){
                    int w,h;
                    if (d.type==DescType::Text||d.type==DescType::Button||d.type==DescType::Title)
                        TTF_SizeUTF8(d.type==DescType::Title?titleFont:font, d.label.c_str(), &w,&h),
                        hh=std::max(hh,h);
                }
                totalH+=hh+(i?SPACING:0);
            } else if (curDesc[i].type == DescType::Image) totalH += curDesc[i].imgH + (i?SPACING:0);
            else if (curDesc[i].type == DescType::Title) {
                int w,h;
                TTF_SizeUTF8(titleFont,curDesc[i].label.c_str(),&w,&h);
                totalH+=h+(i?SPACING:0);
            } else { int w,h;TTF_SizeUTF8(font,curDesc[i].label.c_str(),&w,&h); totalH+=h+(i?SPACING:0); }
        }
        int y = (HEIGHT*SCALE - totalH)/2;
        for (int i = 0; i < n; i++) {
            if (curDesc[i].type == DescType::Toggle) {
                rect[i] = {0,y,WIDTH*SCALE,68+2*V_PADDING};
                y += 68+2*V_PADDING+SPACING;
            } else if (curDesc[i].type == DescType::TextField) {
                rect[i] = {TF_PADDING,y,WIDTH*SCALE-2*TF_PADDING,TF_HEIGHT};
                y += TF_HEIGHT+SPACING;
            } else if (curDesc[i].type == DescType::HStack) {
                int hh=0;
                for(auto&d:hchildren[i]){
                    int w,h;
                    if (d.type==DescType::Text||d.type==DescType::Button||d.type==DescType::Title)
                        TTF_SizeUTF8(d.type==DescType::Title?titleFont:font, d.label.c_str(), &w,&h),
                        hh=std::max(hh,h);
                }
                rect[i] = {0,y,WIDTH*SCALE,hh};
                y += hh+SPACING;
            } else if (curDesc[i].type == DescType::Image) {
                rect[i] = {(WIDTH*SCALE - curDesc[i].imgW)/2, y, curDesc[i].imgW, curDesc[i].imgH};
                y += curDesc[i].imgH + SPACING;
            } else if (curDesc[i].type == DescType::Title) {
                int w,h;
                TTF_SizeUTF8(titleFont,curDesc[i].label.c_str(),&w,&h);
                rect[i] = {(WIDTH*SCALE-w)/2,y,w,h};
                y += h+SPACING;
            } else {
                int w,h;
                TTF_SizeUTF8(font,curDesc[i].label.c_str(),&w,&h);
                rect[i] = {(WIDTH*SCALE-w)/2,y,w,h};
                y += h+SPACING;
            }
        }
        for (int i = 0; i < n; i++) {
            State& st = curStates[i];
            if (curDesc[i].type == DescType::Button) {
                bool d = mouseDown && downX>=rect[i].x&&downX<=rect[i].x+rect[i].w&&downY>=rect[i].y&&downY<=rect[i].y+rect[i].h;
                bool u = mouseUp   && upX  >=rect[i].x&& upX <=rect[i].x+rect[i].w&& upY>=rect[i].y&& upY<=rect[i].y+rect[i].h;
                if (d) { curDesc[i].cb(); st.pressed=true;st.animating=false;st.alpha=PRESSED_ALPHA; }
                if (mouseUp&&st.pressed) { st.pressed=false; if(u){st.animating=true;st.animStart=SDL_GetTicks();}else st.alpha=NORMAL_ALPHA; }
                if (st.animating) {
                    float dt=float(SDL_GetTicks()-st.animStart)/ANIM_DURATION;
                    if (dt>=1) { st.animating=false; st.alpha=NORMAL_ALPHA; }
                    else { float v=springValues[size_t(dt*(springValues.size()-1))]; st.alpha=PRESSED_ALPHA+(NORMAL_ALPHA-PRESSED_ALPHA)*v; }
                }
                SDL_Color col={0,102,255,255};
                SDL_Surface* surf=TTF_RenderUTF8_Blended(font,curDesc[i].label.c_str(),col);
                tex[i]=SDL_CreateTextureFromSurface(renderer,surf);
                SDL_FreeSurface(surf);
                SDL_SetTextureAlphaMod(tex[i],Uint8(st.alpha*255));
                SDL_RenderCopy(renderer,tex[i],nullptr,&rect[i]);
                SDL_DestroyTexture(tex[i]);
            } else if (curDesc[i].type == DescType::Text) {
                SDL_Color col={0,0,0,255};
                SDL_Surface* surf=TTF_RenderUTF8_Blended(font,curDesc[i].label.c_str(),col);
                tex[i]=SDL_CreateTextureFromSurface(renderer,surf);
                SDL_FreeSurface(surf);
                SDL_RenderCopy(renderer,tex[i],nullptr,&rect[i]);
                SDL_DestroyTexture(tex[i]);
            } else if (curDesc[i].type == DescType::Title) {
                SDL_Color col={0,0,0,255};
                SDL_Surface* surf=TTF_RenderUTF8_Blended(titleFont,curDesc[i].label.c_str(),col);
                tex[i]=SDL_CreateTextureFromSurface(renderer,surf);
                SDL_FreeSurface(surf);
                SDL_RenderCopy(renderer,tex[i],nullptr,&rect[i]);
                SDL_DestroyTexture(tex[i]);
            } else if (curDesc[i].type == DescType::Toggle) {
                int ty=rect[i].y;
                int w,h;TTF_SizeUTF8(font,curDesc[i].label.c_str(),&w,&h);
                SDL_Surface* surf=TTF_RenderUTF8_Blended(font,curDesc[i].label.c_str(),SDL_Color{0,0,0,255});
                SDL_Texture* labelTex=SDL_CreateTextureFromSurface(renderer,surf);
                SDL_FreeSurface(surf);
                SDL_Rect lr={5*SCALE,ty+V_PADDING+(68-h)/2,w,h};
                SDL_RenderCopy(renderer,labelTex,nullptr,&lr);
                SDL_DestroyTexture(labelTex);
                int tx=WIDTH*SCALE-5*SCALE-132, ty0=ty+V_PADDING;
                bool& s=*curDesc[i].toggleState;
                int toggW=132,toggH=68,innerPad=5*SCALE;
                int circleD=toggH-2*innerPad;
                bool dT=mouseDown&&downX>=tx&&downX<=tx+toggW&&downY>=ty0&&downY<=ty0+toggH;
                bool uT=mouseUp&&upX>=tx&&upX<=tx+toggW&&upY>=ty0&&upY<=ty0+toggH;
                if(dT){st.togPressed=true;st.togPending=true;st.togTarget=!s;}
                if(mouseUp&&st.togPressed){st.togPressed=false; if(uT){st.togAnimating=true;st.togStart=SDL_GetTicks();}else st.togPending=false;}
                if(!st.togAnimating) st.togPos=s?1.0f:0.0f;
                if(st.togAnimating){
                    float dt=float(SDL_GetTicks()-st.togStart)/ANIM_DURATION;
                    if(dt>=1){st.togAnimating=false; if(st.togPending) s=st.togTarget; st.togPending=false; st.togPos=s?1.0f:0.0f;}
                    else{float r=springValues[size_t(dt*(springValues.size()-1))];st.togPos=st.togTarget?r:(1-r);}
                }
                float vPos=st.togPos;
                SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
                Uint8 r0=0xe9,g0=0xe9,b0=0xeb;
                Uint8 r1=0x69,g1=0xce,b1=0x67;
                Uint8 rc=Uint8(r0+(r1-r0)*vPos), gc=Uint8(g0+(g1-g0)*vPos), bc=Uint8(b0+(b1-b0)*vPos);
                roundedBoxRGBA(renderer,tx,ty0,tx+toggW-1,ty0+toggH-1,toggH/2,rc,gc,bc,255);
                int cx=tx+innerPad+int((toggW-2*innerPad-circleD)*vPos), cy=ty0+innerPad;
                filledCircleRGBA(renderer,cx+circleD/2,cy+circleD/2,circleD/2,0xff,0xff,0xff,255);
            } else if (curDesc[i].type == DescType::TextField) {
                State& stf = curStates[i];
                roundedBoxRGBA(renderer,rect[i].x,rect[i].y,rect[i].x+rect[i].w,rect[i].y+rect[i].h,TF_RADIUS,255,255,255,255);
                roundedRectangleRGBA(renderer,rect[i].x,rect[i].y,rect[i].x+rect[i].w,rect[i].y+rect[i].h,TF_RADIUS,0x88,0x88,0x88,255);
                std::string* txt = curDesc[i].textState;
                bool empty = txt->empty();
                SDL_Color tcol=empty?SDL_Color{0x88,0x88,0x88,255}:SDL_Color{0,0,0,255};
                SDL_Surface* surf=TTF_RenderUTF8_Blended(font,(empty?curDesc[i].label:*txt).c_str(),tcol);
                SDL_Texture* ttx=SDL_CreateTextureFromSurface(renderer,surf);
                int th=surf->h, tw=surf->w;
                SDL_FreeSurface(surf);
                SDL_Rect tr={rect[i].x+5,rect[i].y+(TF_HEIGHT-th)/2,tw,th};
                SDL_RenderCopy(renderer,ttx,nullptr,&tr);
                SDL_DestroyTexture(ttx);
                Uint32 now=SDL_GetTicks();
                if(stf.tfFocused){
                    if(now-stf.lastBlink>CURSOR_BLINK_INTERVAL){
                        stf.showCursor=!stf.showCursor;
                        stf.lastBlink=now;
                    }
                    if(stf.showCursor){
                        int caretX=empty?rect[i].x+5:tr.x+tw;
                        int cy0=rect[i].y+4, cy1=rect[i].y+TF_HEIGHT-4;
                        SDL_RenderDrawLine(renderer,caretX,cy0,caretX,cy1);
                    }
                }
            } else if (curDesc[i].type == DescType::HStack) {
                auto& tmp = hchildren[i];
                int cnt = tmp.size();
                if (!cnt) continue;
                int slotW = WIDTH * SCALE / cnt;
                for (int j = 0; j < cnt; j++) {
                    auto& d = tmp[j];
                    int x0 = j * slotW;
                    int y0 = rect[i].y;
                    if (d.type == DescType::Text || d.type == DescType::Button || d.type == DescType::Title) {
                        SDL_Color col = d.type==DescType::Button?SDL_Color{0,102,255,255}:SDL_Color{0,0,0,255};
                        TTF_Font* f = d.type==DescType::Title? titleFont : font;
                        SDL_Surface* surf = TTF_RenderUTF8_Blended(f, d.label.c_str(), col);
                        SDL_Texture* txr = SDL_CreateTextureFromSurface(renderer, surf);
                        int w,h; SDL_QueryTexture(txr,nullptr,nullptr,&w,&h);
                        SDL_FreeSurface(surf);
                        SDL_Rect rr = {x0 + (slotW-w)/2, y0 + (rect[i].h-h)/2, w,h};
                        SDL_RenderCopy(renderer, txr, nullptr, &rr);
                        SDL_DestroyTexture(txr);
                    }
                }
            } else if (curDesc[i].type == DescType::Image) {
                SDL_Surface* surf = IMG_Load(("./Resources/" + curDesc[i].label).c_str());
                SDL_Texture* imgTex = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_FreeSurface(surf);
                SDL_RenderCopy(renderer, imgTex, nullptr, &rect[i]);
                SDL_DestroyTexture(imgTex);
            }
        }
        SDL_SetRenderTarget(renderer,nullptr);
        SDL_RenderClear(renderer);
        if (animatingOverlay) {
            float dt = float(SDL_GetTicks() - overlayStart)/ANIM_DURATION;
            float t = dt>1?1:dt;
            float v2 = springValues[size_t(t*(springValues.size()-1))];
            if (dt>=1) animatingOverlay = false;
            int offOld = int(-0.5f * WIDTH * SCALE * v2);
            int offNew = int(WIDTH * SCALE * (1 - v2));
            SDL_Rect dstOld = {offOld,0,WIDTH*SCALE,HEIGHT*SCALE};
            SDL_RenderCopy(renderer,nextTarget,nullptr,&dstOld);
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
    SDL_StopTextInput();
    SDL_DestroyTexture(nextTarget);
    SDL_DestroyTexture(currentTarget);
    TTF_CloseFont(titleFont);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}
