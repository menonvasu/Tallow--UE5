// Parlour finish over the house's shared world-aligned surface (M_A6_World). Pixels whose surface sits inside the
// zone box (tested 2 cm out along the normal, so each face of a shared wall belongs to the room it faces) get the
// room's own scanned finish; everything else returns the legacy inputs untouched.
// Inputs: LC LR LN LM LS legacy colour/roughness/normal/metal/specular; P world position; VN vertex normal;
// ZMin/ZMax zone (ZMax.w enables); Tile (u cm, v cm, pattern u cm, pattern v cm); Tint (rgb, saturation);
// Rough (mul, add, metal mul, specular); Grime (dirt height cm, dirt, soot start z, soot); Soot0/Soot1 (centre, radius);
// Mark (soot spot strength, border width cm, border darken, border roughness add); Pat (paper mid luminance, fade,
// pattern strength, aged yellow); Opt (rotate floor uv, soft blend, normal strength, AO strength); Extra (metal add,
// roughness from the scan's own lightness, that lightness's mean).
float3 n = normalize(VN);
float3 q = P + n * 2.0;
float inside = (all(q >= ZMin.xyz) && all(q <= ZMax.xyz)) ? saturate(ZMax.w) : 0.0;
R = LR; Nrm = LN; M = LM; S = LS;
float3 a = abs(n);
float sx = n.x >= 0 ? 1.0 : -1.0;
float sy = n.y >= 0 ? 1.0 : -1.0;
float2 ux = float2(-sx * P.y, -P.z);
float2 uy = float2(sy * P.x, -P.z);
float2 uz = Opt.x > 0.5 ? float2(P.y, -P.x) : float2(P.x, P.y);
float3 du = Opt.x > 0.5 ? float3(0, 1, 0) : float3(1, 0, 0);
float3 dv = Opt.x > 0.5 ? float3(-1, 0, 0) : float3(0, 1, 0);
float3 base = 0, arm = 0, g = 0, pat = 0;
float2 it = 1.0 / max(Tile.xy, 0.001);
float2 ip = 1.0 / max(Tile.zw, 0.001);
if (Opt.y > 0.5)
{
    // Soft triplanar (folded cloth): three projections blended by the normal.
    float3 w = a * a * a * a; w /= max(w.x + w.y + w.z, 1e-5);
    float2 t;
    base = w.x * Texture2DSample(Base, BaseSampler, ux * it).rgb + w.y * Texture2DSample(Base, BaseSampler, uy * it).rgb + w.z * Texture2DSample(Base, BaseSampler, uz * it).rgb;
    arm = w.x * Texture2DSample(ARM, ARMSampler, ux * it).rgb + w.y * Texture2DSample(ARM, ARMSampler, uy * it).rgb + w.z * Texture2DSample(ARM, ARMSampler, uz * it).rgb;
    pat = w.x * Texture2DSample(PatTex, PatTexSampler, ux * ip).rgb + w.y * Texture2DSample(PatTex, PatTexSampler, uy * ip).rgb + w.z * Texture2DSample(PatTex, PatTexSampler, uz * ip).rgb;
    t = Texture2DSample(Norm, NormSampler, ux * it).rg * 2 - 1; g += w.x * (t.x * float3(0, -sx, 0) + t.y * float3(0, 0, -1));
    t = Texture2DSample(Norm, NormSampler, uy * it).rg * 2 - 1; g += w.y * (t.x * float3(sy, 0, 0) + t.y * float3(0, 0, -1));
    t = Texture2DSample(Norm, NormSampler, uz * it).rg * 2 - 1; g += w.z * (t.x * du + t.y * dv);
}
else
{
    // Architecture is square to the axes: one projection, the one the face looks along.
    bool fx = a.x >= a.y && a.x >= a.z, fy = !fx && a.y >= a.z;
    float2 uv = fx ? ux : (fy ? uy : uz);
    float3 tu = fx ? float3(0, -sx, 0) : (fy ? float3(sy, 0, 0) : du);
    float3 tv = (fx || fy) ? float3(0, 0, -1) : dv;
    base = Texture2DSample(Base, BaseSampler, uv * it).rgb;
    arm = Texture2DSample(ARM, ARMSampler, uv * it).rgb;
    pat = Texture2DSample(PatTex, PatTexSampler, uv * ip).rgb;
    float2 t = Texture2DSample(Norm, NormSampler, uv * it).rg * 2 - 1;
    g = t.x * tu + t.y * tv;
}
const float3 L = float3(0.2126, 0.7152, 0.0722);
float3 c = base;
if (Pat.z > 0)
{
    // Printed paper: the pattern carries the colour, the scanned paper carries its stains, tide marks and soil.
    float paper = dot(base, L) / max(Pat.x, 0.02);
    float3 pc = lerp(pat, dot(pat, L).xxx, Pat.y);
    pc *= lerp(float3(1, 1, 1), float3(1.0, 0.86, 0.62), Pat.w);
    c = lerp(base, pc * paper, Pat.z);
}
c = lerp(dot(c, L).xxx, c, Tint.w) * Tint.rgb;
c *= lerp(1.0, arm.r, Opt.w);
float h = P.z - ZMin.z;
float dirt = Grime.y * (1 - smoothstep(0, max(Grime.x, 1), h));
float soot = Grime.w * smoothstep(Grime.z, ZMax.z, P.z);
float s0 = saturate(1 - distance(P, Soot0.xyz) / max(Soot0.w, 1)); 
float s1 = saturate(1 - distance(P, Soot1.xyz) / max(Soot1.w, 1));
soot = saturate(soot + Mark.x * (s0 * s0 + s1 * s1));
float edge = min(min(P.x - ZMin.x, ZMax.x - P.x), min(P.y - ZMin.y, ZMax.y - P.y));
float band = Mark.y > 0 ? 1 - smoothstep(Mark.y * 0.85, Mark.y * 1.15, edge) : 0;
c *= (1 - dirt) * (1 - soot) * (1 - Mark.z * band);
float lift = Extra.y * (dot(base, L) / max(Extra.z, 0.001) - 1);
float r = saturate(arm.g * Rough.x + Rough.y + lift + 0.25 * dirt + 0.2 * soot - Mark.w * band);
g -= n * dot(g, n);
float3 nz = normalize(n + g * Opt.z);
R = lerp(LR, r, inside);
Nrm = normalize(lerp(LN, nz, inside));
M = lerp(LM, saturate(arm.b * Rough.z + Extra.x), inside);
S = lerp(LS, Rough.w, inside);
return lerp(LC, c, inside);
