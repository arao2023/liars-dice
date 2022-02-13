#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <string>
#ifdef __EMSCRIPTEN__
    #include <emscripten.h>
    #include <emscripten/websocket.h>
#endif
#include <SDL.h>
#include <SDL_ttf.h>

namespace SocketWrapper {
    bool isSupported() {
        #ifdef __EMSCRIPTEN__
            return emscripten_websocket_is_supported();
        #else
            //TODO: actually check
            return true;
        #endif
    }
}

class Client {
    private:
        enum class GameState {
            HOME,
            LOBBY,
            INGAME,
        };
        enum class Align {
            NEAR,
            CENTER,
            FAR,
        };

        class FloatRect {
            private:
            public:
                float x {0};
                float y {0};
                float width {0};
                float height {0};
                bool scaled {false};

                SDL_Rect toSDLRect(
                        const int canvasWidth,
                        const int canvasHeight) const {
                    if (scaled) {
                        return SDL_Rect{
                                .x = static_cast<int>(x * canvasWidth),
                                .y = static_cast<int>(y * canvasHeight),
                                .w = static_cast<int>(width * canvasWidth),
                                .h = static_cast<int>(height * canvasHeight)};
                    }
                    else {
                        return SDL_Rect{
                                .x = static_cast<int>(x),
                                .y = static_cast<int>(x),
                                .w = static_cast<int>(width),
                                .h = static_cast<int>(height)};
                    }
                }
        };

        class Text {
            private:
            public:
                std::string text {""};
                FloatRect rect {};
                SDL_Color color {};
                Align horizontalAlign {Align::NEAR};
                Align verticalAlign {Align::NEAR};

                void draw(SDL_Renderer* renderer, TTF_Font* font) const {
                    int canvasWidth;
                    int canvasHeight;
                    SDL_GetRendererOutputSize(
                            renderer, &canvasWidth, &canvasHeight);
                    SDL_Surface* surface {TTF_RenderText_Solid(
                            font, text.c_str(), color)};
                    SDL_Texture* texture {SDL_CreateTextureFromSurface(
                            renderer, surface)};
                    SDL_Rect sdlRect {rect.toSDLRect(
                            canvasWidth, canvasHeight)};
                    switch (horizontalAlign) {
                        case Align::CENTER:
                            sdlRect.x += (sdlRect.w - surface->w) / 2;
                        break;
                        case Align::FAR: 
                            sdlRect.x += sdlRect.w - surface->w;
                        break;
                    }
                    switch (verticalAlign) {
                        case Align::CENTER:
                            sdlRect.y += (sdlRect.h - surface->h) / 2;
                        break;
                        case Align::FAR: 
                            sdlRect.y += sdlRect.h - surface->h;
                        break;
                    }
                    sdlRect.w = surface->w;
                    sdlRect.h = surface->h;
                    SDL_RenderCopy(
                            //                 src=full dst
                            renderer, texture, nullptr, &sdlRect);
                    SDL_FreeSurface(surface);
                }
        };

        class Button {
            private:
            public:
                FloatRect rect {};
                SDL_Color color {};
                bool fill {false};

                bool hasText {false};
                Text text {};

                void draw(SDL_Renderer* renderer, TTF_Font* font) const {
                    int canvasWidth;
                    int canvasHeight;
                    SDL_GetRendererOutputSize(
                            renderer, &canvasWidth, &canvasHeight);
                    SDL_Rect sdlRect {rect.toSDLRect(
                            canvasWidth, canvasHeight)};
                    SDL_SetRenderDrawColor(
                            renderer, color.r, color.g, color.b, color.a);
                    if (fill) {
                        SDL_RenderFillRect(renderer, &sdlRect);
                    }
                    else {
                        SDL_RenderDrawRect(renderer, &sdlRect);
                    }
                    if (hasText) {
                        text.draw(renderer, font);
                    }
                }
        };

        SDL_Window* window;
        SDL_Renderer* renderer;
        TTF_Font* font;

        GameState gameState {GameState::HOME};
        //HOME:
        Button createGameButton {
                .rect = {
                        .x = 0, .y = 0.8, .width = 1, .height = 0.2, 
                        .scaled = true},
                .color = {.r = 128, .g = 255, .b = 128}, 
                .fill = false,
                .hasText = true,
                .text = {
                        .text = "Create Game",
                        .rect = {
                                .x = 0, .y = 0.8, .width = 1, .height = 0.2,
                                .scaled = true},
                        .color = {.r = 128, .g = 255, .b = 128}, 
                        .horizontalAlign = Align::CENTER,
                        .verticalAlign = Align::CENTER}};
        Button joinGameButton {
                .rect = {
                        .x = 0, .y = 0.6, .width = 1, .height = 0.2, 
                        .scaled = true},
                .color = {.r = 255, .g = 255, .b = 128},
                .fill = false,
                .hasText = true,
                .text = {
                        .text = "Join Game",
                        .rect = {
                                .x = 0, .y = 0.6, .width = 1, .height = 0.2,
                                .scaled = true},
                        .color = {.r = 255, .g = 255, .b = 128}, 
                        .horizontalAlign = Align::CENTER,
                        .verticalAlign = Align::CENTER}};

        void draw() {
            switch (gameState) {
                case GameState::HOME:
                    SDL_SetRenderDrawColor(
                            //        r  g  b  a
                            renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
                    SDL_RenderClear(renderer);
                    for (const auto& button : {
                            createGameButton,
                            joinGameButton}) {
                        button.draw(renderer, font);
                    }
                break;
            }
            SDL_RenderPresent(renderer);
        }

    public:
        bool finished {false};

        Client() {
            SDL_Init(SDL_INIT_VIDEO);
            TTF_Init();
            window = SDL_CreateWindow(
                    "Liar's Dice", /* title */
                    SDL_WINDOWPOS_UNDEFINED, /* x */
                    SDL_WINDOWPOS_UNDEFINED, /* y */
                    1024, /* width */
                    1024, /* height */
                    SDL_WINDOW_RESIZABLE);
            renderer = SDL_CreateRenderer(
                    window,
                    -1, /* driver (default) */
                    SDL_RENDERER_ACCELERATED 
                  | SDL_RENDERER_PRESENTVSYNC); /* flags */
            font = TTF_OpenFont("assets/Hack-Regular.ttf", 32);
        }
        ~Client() {
            TTF_Quit();
            SDL_DestroyWindow(window);
            SDL_Quit();
        }

        void tick() {
            SDL_Event event;
            bool redraw {false};
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    /*
                    case SDL_MOUSEBUTTONDOWN:
                        else if (event.button.button == SDL_BUTTON_LEFT) {
                            std::printf("%s", "starting demo tcp client\n");
                            std::printf(
                                    "sockets supported: %s\n", 
                                    SocketWrapper::isSupported() 
                                  ? "yes"
                                  : "no");
                            //TODO: REMOVE
                            {
                                EmscriptenWebSocketCreateAttributes attributes {
                                        //"wss://echo.websocket.events",
                                        "wss://127.0.0.1:443",
                                        nullptr,
                                        true};
                                EMSCRIPTEN_WEBSOCKET_T websocket {
                                        emscripten_websocket_new(&attributes)};
                                emscripten_websocket_set_onopen_callback(
                                        websocket,
                                        nullptr,
                                        [](
                                        int eventType, 
                                        const EmscriptenWebSocketOpenEvent* websocketEvent,
                                        void* userData) -> int {
                                            std::printf("%s", "open event\n");
                                            emscripten_websocket_send_utf8_text(
                                                    websocketEvent->socket,
                                                    "echoing...");
                                            return true;
                                        });
                                emscripten_websocket_set_onerror_callback(
                                        websocket,
                                        nullptr,
                                        [](
                                        int eventType, 
                                        const EmscriptenWebSocketErrorEvent* websocketEvent,
                                        void* userData) -> int {
                                            std::printf("%s", "error event\n");
                                            return true;
                                        });
                                emscripten_websocket_set_onclose_callback(
                                        websocket,
                                        nullptr,
                                        [](
                                        int eventType, 
                                        const EmscriptenWebSocketCloseEvent* websocketEvent,
                                        void* userData) -> int {
                                            std::printf("%s", "close event\n");
                                            return true;
                                        });
                                emscripten_websocket_set_onmessage_callback(
                                        websocket,
                                        nullptr,
                                        [](
                                        int eventType, 
                                        const EmscriptenWebSocketMessageEvent* websocketEvent,
                                        void* userData) -> int {
                                            std::printf("%s", "message event\n");
                                            std::printf("message: %s\n", websocketEvent->data);
                                            emscripten_websocket_close(websocketEvent->socket, 1000, "no reason");
                                            return true;
                                        });
                            }
                            //TODO
                        }
                    */
                    break;
                    case SDL_WINDOWEVENT:
                        redraw = true;
                        switch (event.window.event) {
                            case SDL_WINDOWEVENT_RESIZED:
                            case SDL_WINDOWEVENT_SIZE_CHANGED:
                                /*
                                //Resize font:
                                TTF_CloseFont(font);
                                font = TTF_OpenFont(
                                        "assets/Hack-Regular.ttf", 
                                        //height
                                        event.window.data2 / 64); 
                                */
                            break;
                        }
                    break;
                    case SDL_QUIT:
                        finished = true;
                    break;
                }
                if (redraw) {
                    draw();
                }
            }
        }
};


void mainLoop(void* arg) {
    reinterpret_cast<Client*>(arg)->tick();
}

int main(int argc, char* argv[]) {
    std::printf("%s", "Liar's Dice\n");
    Client client {};

    #ifdef __EMSCRIPTEN__
        emscripten_set_main_loop_arg(
                mainLoop, &client, /* fps */ 0, /* infinite loop? */ true);
    #else
        while (!client.finished) { mainLoop(&client); }
    #endif

    return 0;
}

