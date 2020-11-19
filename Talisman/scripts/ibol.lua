-- Set these to the conference / area you want to post to --
local msgarea = "fsx/dat";
local bbsname = "Talisman"
------------------------------------------------------------

function splittokens(s)
    local res = {}
    for w in s:gmatch("%S+") do
        res[#res+1] = w
    end
    return res
end
 
function textwrap(text, linewidth)
    if not linewidth then
        linewidth = 75
    end
 
    local spaceleft = linewidth
    local res = {}
    local line = {}
 
    for _, word in ipairs(splittokens(text)) do
        if #word + 1 > spaceleft then
            table.insert(res, table.concat(line, ' '))
            line = {word}
            spaceleft = linewidth - #word
        else
            table.insert(line, word)
            spaceleft = spaceleft - (#word + 1)
        end
    end
 
    table.insert(res, table.concat(line, ' '))
    return table.concat(res, '\n')
end

function string.starts(String,Start)
    return string.sub(String,1,string.len(Start))==Start
end

function trim(s)
    return (s:gsub("^%s*(.-)%s*$", "%1"))
end

function stripPipe(s)
    local newstr = "";
    local k;

    k = 0;

    while (k <= string.len(s)) do
        if (string.sub(s, k, k) == "|") then
            k = k + 3;
        else 
            newstr = newstr .. string.sub(s, k, k);
            k = k + 1;
        end
    end

    return newstr;
end

local userhandle;

local postfile;

local i;
local j;

local lastreadfile = bbs_get_data_path() .. "/ibol-lr.dat";
local file = io.open(lastreadfile, "r");
local lastread  = 0

if (file ~= nil) then
    io.input(file);
    lastread = io.read("*n");
    io.close();
end

bbs_clear_screen();

-- bbsname, sysopname, systemname, machinename = bbs_get_info();

userhandle = bbs_get_username();

i = lastread;

postfile = bbs_get_data_path() .. "/ibol.txt";

local file = io.open(postfile, "a");

io.output(file);

local nxt_msg = lastread;

while (nxt_msg ~= 0) do 
    local sender;
    local recipient;
    local subject;
    local msgtext;

    lastread = nxt_msg;

    nxt_msg, sender, recipient, subject, body = bbs_get_message(msgarea, nxt_msg);

    if (subject == "InterBBS Oneliner") then 
        local msgtext = bbs_get_message_body(nxt_msg);
        local thisbbs;
        local thisauthor;
        local thisline = "";
        local finalline = "";

        for line in string.gmatch(msgtext,'[^\r]+') do
            if string.starts(line, "Source:") then 
                thisbbs = trim(stripPipe(string.sub(line, 8, -1)));
            end
            if string.starts(line, "Author:") then 
                thisauthor = trim(stripPipe(string.sub(line, 8, -1)));
            end
            if string.starts(line, "Oneliner:") then 
                thisline = thisline .. trim(stripPipe(string.sub(line, 10, -1))) .. " ";
            end            
        end
       
        finalline = textwrap(thisline, 57);

        j = 0;

        for line in string.gmatch(finalline, '[^\n]+') do
            if (j == 0) then
                io.write(string.format("|12%20.20s|08: |07%s\n", thisauthor, line));
            elseif (j == 1) then
                io.write(string.format("|15%20.20s|08: |07%s\n", thisbbs, line));
            else
                io.write(string.format("                    |08: |07%s\n", line));
            end
            j = j + 1;
        end

        if (j == 0) then
            io.write(string.format("|12%20.20s|08:|07\n", thisauthor));
        elseif (j == 1) then
            io.write(string.format("|15%20.20s|08:|07\n", thisbbs));
        end
        io.write(" |08------------------------------------------------------------------------------|07\n")
    end
end

io.close();

file = io.open(lastreadfile, "w");
io.output(file);
io.write(lastread);

io.close();

local ctr = 0
for _ in io.lines(postfile) do
  ctr = ctr + 1
end

local topline = ctr - 20;

bbs_write_string(" |08+----------------------------------------------------------------------------+|07\r\n");
bbs_write_string(" |08||20  |14>> |15Inter BBS Oneliners |20<<                                                 |16|08||07\r\n");
bbs_write_string(" |08+----------------------------------------------------------------------------+|07\r\n");

ctr = 0;
for line in io.lines(postfile) do
    if (ctr > topline) then
        bbs_write_string(line .. "\r\n");
    end
    ctr = ctr + 1;
end

bbs_write_string("|15(|14A|15)dd (|14V|15)iew or (|14Q|15)uit:|07 ");
local command = bbs_getchar();

if (command == "a" or command == "A") then
    i = 1;
    
    local body = "";

    body = body .. "Author: " .. userhandle .. "\r";
    body = body .. "Source: " .. bbsname .. "\r";

    while (i <= 10) do
        local templine;

        bbs_write_string(string.format("\r\nLine %2d: ", i));
        templine = bbs_read_string(60);

        if (string.len(templine) == 0) then
            break;
        end;
        body = body .. "Oneliner: " .. templine .. "\r";
        i = i + 1;
    end

    if (i > 1) then
        bbs_post_message(msgarea, "IBBS1LINE", userhandle, "InterBBS Oneliner", body);
    end

    bbs_write_string("\r\nPress any key to quit...");
    bbs_getchar();
elseif (command == "v" or command == "V") then
    bbs_write_string("\r\n");
    bbs_display_gfile_pause(postfile);
    bbs_write_string("Press any key to quit...");
    bbs_getchar();
end
