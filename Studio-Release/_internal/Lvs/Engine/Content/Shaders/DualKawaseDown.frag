#version 460

in vec2 UV;
out vec4 FragColor;

uniform sampler2D SourceTexture;
uniform vec2 TexelSize;
uniform float Offset;

void main()
{
    vec2 o = TexelSize * max(0.5, Offset);

    vec4 c = texture(SourceTexture, UV) * 4.0;
    c += texture(SourceTexture, UV + vec2( o.x,  o.y));
    c += texture(SourceTexture, UV + vec2(-o.x,  o.y));
    c += texture(SourceTexture, UV + vec2( o.x, -o.y));
    c += texture(SourceTexture, UV + vec2(-o.x, -o.y));

    FragColor = c * (1.0 / 8.0);
}
