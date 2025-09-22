add_slang_shader(chapter-1-shader
    SOURCE gradient.slang
    ENTRY computeMain
    STAGE comp)

add_slang_shader(gradient_color_shader
    SOURCE gradient_color.slang
    ENTRY computeMain
    STAGE comp)

add_slang_shader(sky_shader
    SOURCE sky.comp
    ENTRY main
    STAGE comp)

add_slang_shader(colored_triangle_vert
    SOURCE colored_triangle.slang
    ENTRY vertexMain
    STAGE vert)

add_slang_shader(colored_triangle_frag
    SOURCE colored_triangle.slang
    ENTRY fragmentMain
    STAGE frag)

add_slang_shader(colored_triangle_mesh_vert
    SOURCE colored_triangle_mesh.slang
    ENTRY vertexMain
    STAGE vert)