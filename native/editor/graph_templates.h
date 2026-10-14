// Constants read from constructors' tables at VA0x108c18..0x108fe8.
// Evidence ev_b1500b260f646c5a75926179c4c25d46e3a31bc9d855a6ac1233f9c8548826ea.
#pragma once
namespace VirusRework::Graphs {
struct TemplateNode { int bx, by, xm, ym; double xoff,yoff,xscale,yscale,x,y,curve; };
inline constexpr TemplateNode BS[] = {
{0,0,0,1,0,64,0,64,0,0,0},
{0,0,0,1,0,64,0,0,0,0,0},
{1,1,2,1,10,0,127,0,0.5,0,0},
{0,0,0,1,10,64,0,0,0,0,0},
{0,0,1,1,147,64,0,0,0,0,0},
};
inline constexpr TemplateNode BP[] = {
{0,0,0,1,0,0,0,0,0,0,0},
{1,1,2,1,20,70,127,57,0.5,0,0},
{0,0,0,1,20,0,0,0,0,0,0},
};
inline constexpr TemplateNode HP[] = {
{0,0,0,1,0,0,0,0,0,0,0},
{1,1,2,0,20,70,127,57,0.5,0,0},
{0,0,0,1,10,70,0,0,0,0,0},
{0,0,1,1,157,70,0,0,0,0,0},
};
inline constexpr TemplateNode LP[] = {
{0,0,0,1,0,70,0,0,0,0,0},
{0,0,0,1,0,70,0,0,0,0,0},
{1,1,2,0,10,0,127,57,0.5,0,0},
{0,0,0,1,20,0,0,0,0,0,0},
};
inline constexpr TemplateNode ENV[] = {
{0,0,0,1,0,0,0,0,0,0,0},
{1,0,0,1,3,127,127,0,0.5,0,0},
{1,0,0,1,2,0,127,0,0.5,0,-0.5},
{0,1,1,2,260,0,0,127,0,0.5,0},
{0,1,0,3,63,-31,0,63,0,0.5,0},
{1,0,0,1,2,0,127,0,0.5,0,-0.5},
};
}
