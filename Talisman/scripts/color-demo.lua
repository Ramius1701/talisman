-- Choose explicitly: ANSI detection does not detect the caller's color depth.
bbs_write_string("|07\r\nArtwork: [1] Classic  [2] 256 colors  [3] Truecolor RGB: ")
local choice = bbs_getchar()
local files = { ["1"] = "color-demo-16", ["2"] = "color-demo-256", ["3"] = "color-demo-truecolor" }
if files[choice] then
  bbs_display_gfile(files[choice])
  bbs_getchar()
end
bbs_write_string("|07\r\n")
