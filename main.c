#include <stdint.h>
#include <stdbool.h>
#include <intrin.h>

#define UNICODE
#include <Windows.h>
#undef UNICODE

#include <d3d11_1.h>
#include <d3dcompiler.h>

#if defined(_MSC_VER) && !defined(__clang__)
#define REAL_MSVC
#endif

// msvc does not properly support restrict
#ifdef REAL_MSVC
#define restrict __restrict
#endif

#include "vec.h"
#include "font.h"
#include "shader.h"

#ifdef REAL_MSVC
#pragma function(memset)
#endif
void *memset(void *dest, int c, size_t count)
{
    char *bytes = (char *)dest;
    while (count-- != 0)
    {
        *bytes++ = (char)c;
    }
    return dest;
}

typedef struct ShaderConstants
{
    float2 player_size;
    float2 player1_position;
    float2 player2_position;
    float2 ball_position;

    float ball_radius;
    float aspect_ratio;

    int unsigned player1_score;
    int unsigned player2_score;
} ShaderConstants;

//char (*__kaboom)[sizeof( struct ShaderConstants )] = 1;

typedef enum PlayerMode
{
    PLAYER1_SERVE,
    PLAYER2_SERVE,
    PLAYER1_FACE,
    PLAYER2_FACE,
} PlayerMode;

typedef struct Player
{
    float2 pos;
    int unsigned score;
} Player;

#define KEY_BITMAP_BIT_SIZE (64)
typedef struct KeyBitmap
{
    uint64_t data[4];
} KeyBitmap;

static inline bool KeyBitmap_get(KeyBitmap const self, int const index)
{
    return ((self.data[index / KEY_BITMAP_BIT_SIZE] >> (index % KEY_BITMAP_BIT_SIZE)) & 1) != 0;
}

static inline void KeyBitmap_flip(KeyBitmap *const this, int const index)
{
    this->data[index / KEY_BITMAP_BIT_SIZE] ^= ((uint64_t)1 << (index % KEY_BITMAP_BIT_SIZE));
}

typedef enum GameMode
{
    GAME_MODE_START,
    GAME_MODE_GAME,
} GameMode;

typedef struct State
{
    HWND window_handle;
    ID3D11Device1 *device;
    ID3D11DeviceContext1 *device_context;

    IDXGISwapChain1 *swap_chain;

    ID3D11Texture2D *frame_buffer;
    ID3D11RenderTargetView *frame_buffer_view;

    ID3D11VertexShader *vertex_shader;
    ID3D11PixelShader *pixel_shader;

    ID3D11Buffer *constant_buffer;

    ID3D11Texture2D* texture;
    ID3D11ShaderResourceView *texture_view;
    ID3D11SamplerState *sampler_state;

    int width;
    int height;

    float2 ball_position;
    float2 ball_velocity;

    Player player1;
    Player player2;
    PlayerMode player_mode;

    KeyBitmap keys;

    GameMode game_mode;
} State;

#define BALL_RADIUS (0.025f)
#define PLAYER_SIZE ((float2){0.05f, 0.24f})
#define INITIAL_BALL_VELOCITY ((float2){.x = 0.01f, .y = 0.0f})

static LRESULT __stdcall WindowProc(HWND const window_handle, UINT const message,
                                    WPARAM const wParam, LPARAM const lParam)
{
    // see https://docs.microsoft.com/en-us/windows/win32/learnwin32/managing-application-state-
    if (message == WM_NCCREATE)
    {
        SetWindowLongPtrW(window_handle, GWLP_USERDATA,
                          (LONG_PTR)((CREATESTRUCTW*)lParam)->lpCreateParams);
    }

    if (window_handle == NULL)
    {
        return DefWindowProcW(window_handle, message, wParam, lParam);
    }

    State *const this = (State*)(uintptr_t)GetWindowLongPtrW(window_handle, GWLP_USERDATA);

    switch (message)
    {
        case WM_SYSCHAR:
        case WM_SYSKEYDOWN:
        {
            // check for alt-f4
            // if the 29-th bit is set the alt is key was pressed
            if (((lParam >> 29) & 0x1) != 0 && wParam == VK_F4)
            {
                PostQuitMessage(0);
            }
            break;
        }

        case WM_SYSKEYUP:
        {
            break;
        }

        // see https://stackoverflow.com/a/28096992
        case WM_SIZE:
        {
            this->width = (int) lParam & 0xFFFF;
            this->height = ((int) lParam >> 16) & 0xFFFF;
            if (this->width == 0 || this->height == 0 || this->device == NULL) break;


            // unbind render target
            this->device_context->lpVtbl->OMSetRenderTargets(this->device_context, 1,
                                                             &(ID3D11RenderTargetView *) {NULL}, NULL);

            // release frame_buffer view
            this->frame_buffer_view->lpVtbl->Release(this->frame_buffer_view);

            // release frame_buffer
            this->frame_buffer->lpVtbl->Release(this->frame_buffer);

            // let things finish
            this->device_context->lpVtbl->Flush(this->device_context);

            this->swap_chain->lpVtbl->ResizeBuffers(this->swap_chain, 1,
                                                    (int unsigned) this->width,
                                                    (int unsigned) this->height,
                                                    DXGI_FORMAT_B8G8R8A8_UNORM, 0);

            this->swap_chain->lpVtbl->GetBuffer(this->swap_chain, 0,
                                                &IID_ID3D11Texture2D,
                                                (void **) &this->frame_buffer);

            this->device->lpVtbl->CreateRenderTargetView(this->device,
                                                         (ID3D11Resource *) this->frame_buffer,
                                                         NULL, &this->frame_buffer_view);

            break;
        }

        case WM_QUIT:
        case WM_CLOSE:
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            break;
        }

        case WM_MOUSEMOVE:
        {
            int const mouse_y = ((int) lParam >> 16) & 0xFFFF;

            // clamp the player position so it does not go off the screen
            float const new_player_height = fclamp(1.0f - (float) mouse_y / (float) this->height,
                                                   PLAYER_SIZE.y / 2.0f, 1.0f - PLAYER_SIZE.y / 2.0f);

            this->player1.pos.y = new_player_height;

            break;
        }

        case WM_KEYUP:
        case WM_KEYDOWN:
        {
            if (((lParam >> 30) & 0x1) == ((lParam >> 31) & 0x1))
            {
                KeyBitmap_flip(&this->keys, wParam);
            }

            break;
        }

        default:
        {
            return DefWindowProcW(window_handle, message, wParam, lParam);
        }
    }

    return 0;
}

static void State_create_window(State *const this,
                                int const width,
                                int const height,
                                wchar_t const *const title)
{
    HMODULE const instance_handle = GetModuleHandleW(NULL);

    this->width = width;
    this->height = height;

    RegisterClassW(&(WNDCLASSW) {
        .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .lpfnWndProc = &WindowProc,
        .hInstance = instance_handle,
        .hCursor = LoadCursorW(NULL, IDC_ARROW),
        .lpszClassName = L" ",
    });

    this->window_handle = CreateWindowExW(0, L" ", title,
                                          WS_OVERLAPPEDWINDOW,
                                          CW_USEDEFAULT, CW_USEDEFAULT,
                                          this->width, this->height,
                                          NULL, NULL, instance_handle, this);

    ShowWindow(this->window_handle, SW_SHOWDEFAULT);
}

static void State_setup_d3d(State *const this)
{
    D3D_FEATURE_LEVEL const feature_levels[] = {D3D_FEATURE_LEVEL_11_1};

    ID3D11Device *base_device;
    ID3D11DeviceContext *base_device_context;

    D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                      D3D11_CREATE_DEVICE_BGRA_SUPPORT, feature_levels,
                      sizeof(feature_levels) / sizeof(*feature_levels),
                      D3D11_SDK_VERSION, &base_device, NULL, &base_device_context);

    base_device->lpVtbl->QueryInterface(base_device, &IID_ID3D11Device1,
                                        (void **) &this->device);

    base_device_context->lpVtbl->QueryInterface(base_device_context,
                                                &IID_ID3D11DeviceContext1,
                                                (void **) &this->device_context);

    IDXGIDevice1 *dxgi_device;
    this->device->lpVtbl->QueryInterface(this->device, &IID_IDXGIDevice1,
                                         (void **) &dxgi_device);

    IDXGIAdapter *dxgi_adapter;
    dxgi_device->lpVtbl->GetAdapter(dxgi_device, &dxgi_adapter);

    IDXGIFactory2 *dxgi_factory;
    dxgi_adapter->lpVtbl->GetParent(dxgi_adapter, &IID_IDXGIFactory2,
                                    (void **) &dxgi_factory);

    dxgi_factory->lpVtbl->CreateSwapChainForHwnd(dxgi_factory, (IUnknown *) this->device,
                                                 this->window_handle,
                                                 &(DXGI_SWAP_CHAIN_DESC1) {
                                                     .Width = 0, // use window width
                                                     .Height = 0, // use window height
                                                     .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                                                     .Stereo = FALSE,
                                                     .SampleDesc.Count = 1,
                                                     .SampleDesc.Quality = 0,
                                                     .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
                                                     .BufferCount = 2,
                                                     .Scaling = DXGI_SCALING_STRETCH,
                                                     .SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
                                                     .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED
                                                 }, NULL, NULL, &this->swap_chain);

    this->swap_chain->lpVtbl->GetBuffer(this->swap_chain, 0,
                                        &IID_ID3D11Texture2D,
                                        (void **) &this->frame_buffer);

    this->device->lpVtbl->CreateRenderTargetView(this->device,
                                                 (ID3D11Resource *) this->frame_buffer,
                                                 NULL, &this->frame_buffer_view);

    ID3DBlob *error_blob;
    ID3DBlob *vertex_shader_blob;

    HRESULT result = D3DCompile(shader_program, sizeof(shader_program), "main.hlsl",
                                NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                                "vs_main", "vs_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
                                &vertex_shader_blob, &error_blob);
    #ifndef RELEASE_BUILD
    if (FAILED(result))
    {
        MessageBoxA(this->window_handle, error_blob->lpVtbl->GetBufferPointer(error_blob), "error:", MB_OK);
        ExitProcess(1);
    }
    #endif

    (void)error_blob;

    void *const vertex_shader_blob_buffer_pointer =
        vertex_shader_blob->lpVtbl->GetBufferPointer(vertex_shader_blob);

    size_t const vertex_shader_blob_buffer_size =
        vertex_shader_blob->lpVtbl->GetBufferSize(vertex_shader_blob);

    this->device->lpVtbl->CreateVertexShader(this->device,
                                             vertex_shader_blob_buffer_pointer,
                                             vertex_shader_blob_buffer_size,
                                             NULL, &this->vertex_shader);

    ID3DBlob *pixel_shader_blob;
    result = D3DCompile(shader_program, sizeof(shader_program), "main.hlsl",
                        NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                        "ps_main", "ps_5_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
                        &pixel_shader_blob, &error_blob);

    #ifndef RELEASE_BUILD
    if (FAILED(result))
    {
        MessageBoxA(this->window_handle, error_blob->lpVtbl->GetBufferPointer(error_blob), "error:", MB_OK);
        ExitProcess(1);
    }
    #endif

    this->device->lpVtbl->CreatePixelShader(this->device,
                                            pixel_shader_blob->lpVtbl->GetBufferPointer(pixel_shader_blob),
                                            pixel_shader_blob->lpVtbl->GetBufferSize(pixel_shader_blob),
                                            NULL, &this->pixel_shader);

    this->device->lpVtbl->CreateBuffer(this->device,
                                       &(D3D11_BUFFER_DESC) {
                                           .ByteWidth = (int unsigned) sizeof(ShaderConstants),
                                           .Usage = D3D11_USAGE_DYNAMIC,
                                           .BindFlags  = D3D11_BIND_CONSTANT_BUFFER,
                                           .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
                                       }, NULL, &this->constant_buffer);

    this->device->lpVtbl->CreateTexture2D(this->device,
                                          &(D3D11_TEXTURE2D_DESC) {
                                              .Width = TEXTURE_WIDTH,
                                              .Height = TEXTURE_HEIGHT,
                                              .MipLevels = 1,
                                              .ArraySize = 1,
                                              .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                                              .SampleDesc.Count = 1,
                                              .Usage = D3D11_USAGE_IMMUTABLE,
                                              .BindFlags = D3D11_BIND_SHADER_RESOURCE
                                          },
                                          &(D3D11_SUBRESOURCE_DATA) {
                                              .pSysMem = font_texture_bin,
                                              .SysMemPitch = TEXTURE_WIDTH * 4
                                          }, &this->texture);

    this->device->lpVtbl->CreateShaderResourceView(this->device, (ID3D11Resource *) this->texture,
                                                   NULL, &this->texture_view);

    this->device->lpVtbl->CreateSamplerState(this->device,
                                             &(D3D11_SAMPLER_DESC) {
                                                 .Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
                                                 .AddressU       = D3D11_TEXTURE_ADDRESS_WRAP,
                                                 .AddressV       = D3D11_TEXTURE_ADDRESS_WRAP,
                                                 .AddressW       = D3D11_TEXTURE_ADDRESS_WRAP,
                                                 .ComparisonFunc = D3D11_COMPARISON_NEVER
                                             }, &this->sampler_state);
}

static void State_draw(State *const this)
{
    // clear background color to black
    this->device_context->lpVtbl->ClearRenderTargetView(this->device_context,
                                                        this->frame_buffer_view,
                                                        (float[4]) {[3] = 1.0f});

    // get the size of the portion of the window that we can draw to
    this->device_context->lpVtbl->RSSetViewports(this->device_context, 1,
                                                 &(D3D11_VIEWPORT) {
                                                     .Width = (float)this->width,
                                                     .Height = (float)this->height,
                                                     .MinDepth = 0.0f,
                                                     .MaxDepth = 1.0f,
                                                 });

    this->device_context->lpVtbl->OMSetRenderTargets(this->device_context, 1, &this->frame_buffer_view, NULL);

    this->device_context->lpVtbl->IASetPrimitiveTopology(this->device_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    this->device_context->lpVtbl->VSSetShader(this->device_context, this->vertex_shader, NULL, 0);
    this->device_context->lpVtbl->PSSetShader(this->device_context, this->pixel_shader, NULL, 0);

    this->device_context->lpVtbl->PSSetConstantBuffers(this->device_context, 0, 1, &this->constant_buffer);
    this->device_context->lpVtbl->PSSetShaderResources(this->device_context, 0, 1, &this->texture_view);
    this->device_context->lpVtbl->PSSetSamplers(this->device_context, 0, 1, &this->sampler_state);

    // draw the shaders and swap the front/back buffer
    this->device_context->lpVtbl->Draw(this->device_context, 4, 0);
    this->swap_chain->lpVtbl->Present(this->swap_chain, 1, 0);
}



static void State_update_ai(State *const this, Player *const player, float const dt)
{
    float const correct_width = (float) this->width / (float) this->height;
    float const dy = 0.25f * dt;

    if (this->ball_position.x > correct_width / 2 && this->ball_velocity.x > 0)
    {
        if (player->pos.y < this->ball_position.y - dy)
        {
            player->pos.y += dy;
        }
        else if (player->pos.y > this->ball_position.y + dy)
        {
            player->pos.y -= dy;
        }
    }
    else
    {
        if (player->pos.y < 0.5f - dy)
        {
            player->pos.y += dy;
        }
        else if (player->pos.y > 0.5f + dy)
        {
            player->pos.y -= dy;
        }
    }

    player->pos.y = fclamp(player->pos.y, PLAYER_SIZE.y / 2.0f, 1.0f - PLAYER_SIZE.y / 2.0f);
}

static inline void State_reset(State *const this)
{
    this->player1.pos.x = 0.1f;
    this->player1.pos.y = 0.5f;
    this->player2.pos.y = 0.5f;

    this->player1.score = 0;
    this->player2.score = 0;
    this->player_mode = __rdtsc() % 2 != 0 ? PLAYER1_SERVE : PLAYER2_SERVE;

    this->game_mode = GAME_MODE_START;
}

static void State_update(State *const this, float const frame_delta)
{
    if (KeyBitmap_get(this->keys, 'R'))
    {
        State_reset(this);
    }

    float const correct_width = (float) this->width / (float) this->height;

    this->ball_position.x += this->ball_velocity.x * frame_delta;
    this->ball_position.y += this->ball_velocity.y * frame_delta;

    this->player2.pos.x = correct_width - this->player1.pos.x;

    State_update_ai(this, &this->player2, 0.043f * frame_delta);


    #define BOUNCE_STRENGTH (1.25f)
    switch (this->player_mode)
    {
        case PLAYER1_SERVE:
        {
            this->ball_velocity = (float2) {0};
            this->ball_position = (float2) {this->player1.pos.x + PLAYER_SIZE.x, this->player1.pos.y};
            if (KeyBitmap_get(this->keys, ' '))
            {
                this->player_mode = PLAYER2_FACE;
                this->ball_velocity = INITIAL_BALL_VELOCITY;
            }

            break;
        }

        case PLAYER2_SERVE:
        {
            this->ball_velocity = (float2) {0};
            this->ball_position = (float2) {this->player2.pos.x - PLAYER_SIZE.x, this->player2.pos.y};

            if (this->game_mode == GAME_MODE_GAME || KeyBitmap_get(this->keys, ' '))
            {
                this->player_mode = PLAYER1_FACE;
                this->ball_velocity = (float2) {-INITIAL_BALL_VELOCITY.x, INITIAL_BALL_VELOCITY.y};

                this->game_mode = GAME_MODE_GAME;
            }

            break;
        }

        case PLAYER1_FACE:
        {
            if (this->ball_position.x - BALL_RADIUS <= this->player1.pos.x + PLAYER_SIZE.x / 2 &&
                fabsf(this->ball_position.y - this->player1.pos.y) <= PLAYER_SIZE.y - BALL_RADIUS * 2.0f)
            {
                float const percentage = (this->ball_position.y - this->player1.pos.y) / (PLAYER_SIZE.y / 2.0f);
                this->ball_velocity.x *= -1.0f;
                this->ball_velocity.y = INITIAL_BALL_VELOCITY.x * percentage * BOUNCE_STRENGTH;

                this->player_mode = PLAYER2_FACE;
            }

            break;
        }
        
        case PLAYER2_FACE:
        {
            if (this->ball_position.x + BALL_RADIUS >= this->player2.pos.x - PLAYER_SIZE.x / 2 &&
                fabsf(this->ball_position.y - this->player2.pos.y) <= PLAYER_SIZE.y - BALL_RADIUS * 2.0f)
            {
                float const percentage = (this->ball_position.y - this->player2.pos.y) / (PLAYER_SIZE.y / 2.0f);
                this->ball_velocity.x *= -1.0f;
                this->ball_velocity.y = INITIAL_BALL_VELOCITY.x * percentage * BOUNCE_STRENGTH;

                this->player_mode = PLAYER1_FACE;
            }

            break;
        }
    }
    #undef BOUNCE_STRENGTH

    if (this->ball_position.y - BALL_RADIUS < 0 || this->ball_position.y + BALL_RADIUS >= 1)
    {
        this->ball_velocity.y *= -1.0f;
    }

    if (this->ball_position.x - BALL_RADIUS < 0)
    {
        ++this->player2.score;
        this->player_mode = PLAYER2_SERVE;
    }
    else if(this->ball_position.x + BALL_RADIUS >= correct_width)
    {
        ++this->player1.score;
        this->player_mode = PLAYER1_SERVE;
    }

    this->ball_position.y = fclamp(this->ball_position.y, BALL_RADIUS, 1.0f - BALL_RADIUS);
}

__declspec(noreturn) void entry(void);
__declspec(noreturn) void entry(void)
{
    static State state;
    State_create_window(&state, 900, 600, L"pong");
    State_setup_d3d(&state);

    State_reset(&state);

    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);

    uint64_t time_last = __rdtsc();

    for (;;)
    {
        uint64_t const time_now = __rdtsc();
        float const frame_delta = (float)((double)(time_now - time_last) / (double)frequency.QuadPart) / 2.3333f;
        time_last = __rdtsc();

        MSG message;
        if (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);

            if (message.message == WM_QUIT) break;
        }

        // TODO: properly handle minimization
        if (state.width == 0 || state.height == 0) continue;

        D3D11_MAPPED_SUBRESOURCE mapped_subresource;
        state.device_context->lpVtbl->Map(state.device_context,
                                          (ID3D11Resource *) state.constant_buffer, 0,
                                          D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource);

        ShaderConstants *const shader_constants = mapped_subresource.pData;

        shader_constants->player_size = PLAYER_SIZE;
        shader_constants->player1_position = state.player1.pos;
        shader_constants->player2_position = state.player2.pos;

        shader_constants->ball_position = state.ball_position;
        shader_constants->ball_radius = BALL_RADIUS;
        shader_constants->aspect_ratio = (float) state.width / (float) state.height;

        shader_constants->player1_score = state.player1.score;
        shader_constants->player2_score = state.player2.score;

        state.device_context->lpVtbl->Unmap(state.device_context,
                                            (ID3D11Resource *) state.constant_buffer, 0);

        State_draw(&state);

        State_update(&state, frame_delta);

        // remove warnings
        (void)shader_constants->player_size;
        (void)shader_constants->ball_radius;
        (void)shader_constants->aspect_ratio;
        (void)shader_constants->ball_position;
        (void)shader_constants->player1_position;
        (void)shader_constants->player2_position;
        (void)shader_constants->player1_score;
        (void)shader_constants->player2_score;
    }

    ExitProcess(0);
}

