#pragma once

static char const shader_program[] =
    "cbuffer constants : register (b0)\n"
    "{\n"
    "    float2 player_size;\n"
    "    float2 player1_position;\n"
    "    float2 player2_position;\n"
    "    float2 ball_position;\n"
    "\n"
    "    float ball_radius;\n"
    "    float aspect_ratio;\n"
    "\n"
    "    uint player1_score;\n"
    "    uint player2_score;\n"
    "}\n"
    "\n"
    "struct vs_out\n"
    "{\n"
    "    float4 position : SV_POSITION;\n"
    "    float2 texture_coord : TEX;\n"
    "};\n"
    "\n"
    "Texture2D font_texture : register(t0);\n"
    "SamplerState font_sampler : register(s0);\n"
    "\n"
    "vs_out vs_main(uint vertex_index : SV_VERTEXID)\n"
    "{\n"
    "     vs_out output;\n"
    "\n"
    "     float2 texture_coord = float2(vertex_index & 1,vertex_index >> 1);\n"
    "     output.position = float4((texture_coord.x - 0.5f) * 2, -(texture_coord.y - 0.5f) * 2, 0, 1);\n"
    "     output.texture_coord = 0.5f * output.position.xy + 0.5f;\n"
    "\n"
    "     return output;\n"
    "}\n"
    "\n"
    "// see https://www.iquilezles.org/www/articles/smin/smin.htm\n"
    "float smin(float a, float b, float k)\n"
    "{\n"
    "    float h = max(k - abs(a - b), 0.0f) / k;\n"
    "    return min(a, b) - h * h * h * k * (1.0f / 6.0f);\n"
    "}\n"
    "\n"
    "float circle_sdf(float2 coords,\n"
    "                 float2 position,\n"
    "                 float radius)\n"
    "{\n"
    "    return length(coords - position) - radius;\n"
    "}\n"
    "\n"
    "float rectangle_sdf(float2 coords, float2 position, float2 half_size)\n"
    "{\n"
    "    float2 component_wise_edge_distance = abs(coords - position) - half_size;\n"
    "    float outside_distance = length(max(component_wise_edge_distance, 0));\n"
    "    float inside_distance = min(max(component_wise_edge_distance.x, component_wise_edge_distance.y), 0);\n"
    "    return outside_distance + inside_distance;\n"
    "}\n"
    "\n"
    "float bl_rectangle_sdf(float2 coords, float2 position, float2 size)\n"
    "{\n"
    "    float2 half_size = size / 2.0f;\n"
    "    return rectangle_sdf(coords, position + half_size, half_size);\n"
    "}\n"
    "\n"
    "float sdf_to_mask(float sdf)\n"
    "{\n"
    "    return smoothstep(0.002f, 0.001f, sdf);\n"
    "}\n"
    "\n"
    "float median(float4 value)\n"
    "{\n"
    "    return (value.x + value.y + value.z + value.w) - min(value.x, min(value.y, min(value.z, value.w))) - max(value.x, max(value.y, max(value.z, value.w)));\n"
    "}\n"
    "\n"
    "float number_text_mask(float2 coords, float2 position, float2 scale, uint number, float number_log)\n"
    "{\n"
    "    coords -= position;\n"
    "    coords *= scale;\n"
    "\n"
    "    const float2 offsets[] = {\n"
    "        float2(0.225f * 0.0f, 0.3375f * 2.0f), // 0\n"
    "        float2(0.225f * 3.0f, 0.3375f * 2.0f), // 1\n"
    "        float2(0.225f * 2.0f, 0.3375f * 1.0f), // 2\n"
    "        float2(0.225f * 3.0f, 0.3375f * 1.0f), // 3\n"
    "        float2(0.225f * 1.0f, 0.3375f * 0.0f), // 4\n"
    "        float2(0.225f * 1.0f, 0.3375f * 1.0f), // 5\n"
    "        float2(0.225f * 0.0f, 0.3375f * 1.0f), // 6\n"
    "        float2(0.225f * 0.0f, 0.3375f * 0.0f), // 7\n"
    "        float2(0.225f * 2.0f, 0.3375f * 2.0f), // 8\n"
    "        float2(0.225f * 1.0f, 0.3375f * 2.0f), // 9\n"
    "    };\n"
    "\n"
    "    const float kerning[] = {\n"
    "        0.005f, // 0\n"
    "        0.0085f, // 1\n"
    "        0.04f, // 2\n"
    "        0.05f, // 3\n"
    "        0.015f, // 4\n"
    "        0.02f, // 5\n"
    "        0.0f, // 6\n"
    "        0.0f, // 7\n"
    "        0.03f, // 8\n"
    "        0.0f, // 9\n"
    "    };\n"
    "\n"
    "    float result = 0.0f;\n"
    "\n"
    "    for(float i = 0.0f; i < 9; ++i)\n"
    "    {\n"
    "        uint digit = number % 10;\n"
    "        float letter_mask = median(font_texture.Sample(font_sampler,\n"
    "                                                       coords -\n"
    "                                                       float2(0.225f, 0.0f) * float(number_log - 1 - i) +\n"
    "                                                       offsets[digit] + float2(kerning[digit], 0.0f)));\n"
    "\n"
    "\n"
    "        letter_mask = min(letter_mask,\n"
    "                          sdf_to_mask(bl_rectangle_sdf(coords,\n"
    "                                                       float2(0.225f, 0.0f) * float(number_log - 1 - i),\n"
    "                                                       float2(0.2365f, 0.3375f))));\n"
    "\n"
    "        result = max(result, letter_mask);\n"
    "        number /= 10;\n"
    "\n"
    "        if (number == 0) break;\n"
    "    }\n"
    "\n"
    "    return smoothstep(1.0f - 0.5f, 1.0f, result);\n"
    "}\n"
    "\n"
    "float4 ps_main(vs_out input) : SV_TARGET\n"
    "{\n"
    "    const float2 scale_correction = float2(aspect_ratio, 1.0f);\n"
    "    float2 coords = input.texture_coord * scale_correction;\n"
    "\n"
    "    float ball = circle_sdf(coords, ball_position, ball_radius);\n"
    "    float player1 = rectangle_sdf(coords, player1_position, player_size / 2.0f);\n"
    "    float player2 = rectangle_sdf(coords, player2_position, player_size / 2.0f);\n"
    "    float middle_line = abs(coords.x - 0.5f * aspect_ratio) - 0.005f;\n"
    "    float middle_line_pattern = fmod(abs(coords.y) + 0.01f, 0.1f) - 0.025f;\n"
    "    middle_line = max(middle_line, -middle_line_pattern);\n"
    "\n"
    "    float final_sdf = smin(ball, min(player1, player2), 0.05f);\n"
    "\n"
    "    float3 final_color = 0;\n"
    "    final_color = lerp(final_color, float3(1, 0, 0), smoothstep(player1 - 0.05f, player1, final_sdf));\n"
    "    final_color = lerp(final_color, float3(0, 1, 0), smoothstep(player2 - 0.05f, player2, final_sdf));\n"
    "    final_color = lerp(final_color, float3(0, 0, 1), smoothstep(ball - 0.05f, ball, final_sdf));\n"
    "\n"
    "    float player1_score_log10 = floor(log(player1_score + 1) / log(10.0f));\n"
    "    float player2_score_log10 = floor(log(player2_score + 1) / log(10.0f));\n"
    "    float player1_score_mask = number_text_mask(coords, float2(0.085f, 0.87f), 5.0f, player1_score, player1_score_log10);\n"
    "    float player2_score_mask = number_text_mask(coords, float2(aspect_ratio - 0.075f -\n"
    "                                                               player2_score_log10 * 0.225f / 5.0f, 0.87f),\n"
    "                                                5.0f, player2_score, player2_score_log10);\n"
    "\n"
    "    float overlay_mask = max(max(player1_score_mask, player2_score_mask), sdf_to_mask(middle_line));\n"
    "    float final_mask = sdf_to_mask(final_sdf.x);\n"
    "\n"
    "    return float4(lerp(overlay_mask, final_color, final_mask), 1.0f);\n"
    "}";