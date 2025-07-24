// serviettUI.h
#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include <vector>
#include <string>
#include <functional>

void Text(const std::string& text);
void Button(const std::string& label, const std::function<void()>& cb);
void Toggle(const std::string& label, bool& state);
void TextField(const std::string& placeholder, std::string& state);
void HStack(const std::function<void()>& cb);
void NewView(const std::function<void()>& viewFunc);
void Image(const std::string& path, int w, int h);
void View(const std::function<void()>& viewFunc);
