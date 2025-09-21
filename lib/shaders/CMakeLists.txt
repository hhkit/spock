add_slang_shader(chapter-1-shader
    SOURCE gradient.comp.slang
    ENTRY computeMain)

add_slang_shader(gradient_color_shader
    SOURCE gradient_color.comp.slang
    ENTRY computeMain)

add_slang_shader(sky_shader
    SOURCE sky.comp
    ENTRY main)
