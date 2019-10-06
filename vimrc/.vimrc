set number " 设置显示行号
set tabstop=4 " tab 键设置为4个空格
set shiftwidth=4 " 在文本上按下>>（增加一级缩进）、<<（取消一级缩进）或者==（取消全部缩进）时，每一级的字符数
set expandtab " 设置自动将 Tab 转为空格
set showmode " 在底部显示，当前处于命令模式还是插入模式
set showcmd " 命令模式下，在底部显示，当前键入的指令
set softtabstop=4 " Tab 转为多少个空格
set scrolloff=1 " 设置滚屏时保留1行
set textwidth=80 " 设置行宽，即一行显示多少个字符
set wrap " 自动折行，即太长的行分成几行显示
set hlsearch " 搜索时，高亮显示匹配结果
set incsearch " 输入搜索模式时，每输入一个字符，就自动跳到第一个匹配的结果
set autoindent " 自动缩进
set laststatus=2 " 是否显示状态栏。0 表示不显示，1 表示只在多窗口时显示，2 表示显示
set ruler " 在状态栏显示光标的当前位置（位于哪一行哪一列）
set noshowmatch " 光标遇到圆括号、方括号、大括号时，自动高亮对应的另一个圆括号、方括号和大括号
set ignorecase " 搜索时忽略大小写
set smartcase " 如果同时打开了ignorecase，那么对于只有一个大写字母的搜索词，将大小写敏感；其他情况都是大小写不敏感
set smartindent " 打开智能缩进
set fdm=indent " 缩进折叠
set clipboard=unnamed "设置复制到系统剪贴板
set cindent
set undofile " 保留撤销历史
set autochdir " 该配置可以将工作目录自动切换到，正在编辑的文件的目录
set noerrorbells " 出错时，不要发出响声
set history=1000 " Vim 需要记住多少次历史操作
set autoread  " 打开文件监视。如果在编辑过程中文件发生外部改变（比如被别的编辑器编辑了），就会发出提示
set nocompatible              " 去除VI一致性,必须
set wildmenu
set wildmode=longest:list,full " 命令模式下，底部操作指令按下 Tab 键自动补全。第一次按下 Tab，会显示所有匹配的操作指令的清单；第二次按下 Tab，会依次选择各个指令
set background=dark
set fileencodings=utf-8,gbk
set termencoding=utf-8
set encoding=utf-8
set t_Co=256
syntax on " 打开语法检查

" map
let mapleader=',' " 设置leader键
inoremap jj <Esc>
nnoremap qn :q!<CR>
nnoremap wn :w<CR>

" 设置光标窗口移动快捷键
noremap <C-h> <C-w>h
noremap <C-j> <C-w>j
noremap <C-k> <C-w>k
noremap <C-l> <C-w>l

" 切换buffer
nnoremap <silent> [b :bprevious<CR>
nnoremap <silent> [n :bnext<CR>
nnoremap <silent> [d :bdelete<CR>

" 简要帮助文档
" :PluginList       - 列出所有已配置的插件
" :PluginInstall    - 安装插件,追加 `!` 用以更新或使用 :PluginUpdate
" :PluginSearch foo - 搜索 foo ; 追加 `!` 清除本地缓存
" :PluginClean      - 清除未使用插件,需要确认; 追加 `!` 自动批准移除未使用插件
" 查阅 :h vundle 获取更多细节和wiki以及FAQ
" 设置包括vundle和初始化相关的runtime path
filetype off
set rtp+=~/.vim/bundle/Vundle.vim
call vundle#begin()
Plugin 'VundleVim/Vundle.vim'
Bundle 'Yggdroot/indentLine'
Bundle 'mhinz/vim-startify'
Bundle 'brooth/far.vim'
Bundle 'godlygeek/tabular'
Bundle 'tpope/vim-fugitive'
Bundle 'junegunn/gv.vim'
Bundle 'tpope/vim-commentary'
Bundle 'ntpeters/vim-better-whitespace'
Plugin 'lfv89/vim-interestingwords'

Bundle 'easymotion/vim-easymotion'
nmap ss <Plug>(easymotion-s2)

Bundle 'tpope/vim-surround'
" ys iw " 单词间添加双引号
" cs " ' 替换双引号为单引号
" ds ' 删除单引号中的内容

Bundle 'Lokaltog/vim-powerline'
let g:Powerline_symbols = 'fancy'

Bundle 'vim-airline/vim-airline-themes'
Bundle 'vim-airline/vim-airline'
let g:airline_theme='luna'
let g:airline#extensions#tabline#enabled = 1
let g:airline#extensions#tabline#left_sep = ' '
let g:airline#extensions#tabline#left_alt_sep = '|'

Bundle 'w0ng/vim-hybrid'
let g:hybrid_reduced_contrast = 1

Bundle 'scrooloose/nerdtree'
let NERDTreeShowHidden=1
let NERDTreeAutoDeleteBuffer=1
let NERDTreeWinSize=30
let NERDTreeIgnore = ['\.swp$', '\.o$', '\.un\~$', '\.git$', '\.pyc$', '\.svn$', '\.out$']
map <leader>n :NERDTreeToggle<CR>
map <leader>f :NERDTreeFind<CR>

Bundle 'majutsushi/tagbar'
let g:tagbar_width = 30
map <leader>t :TagbarToggle<CR>
map <leader>c :!ctags -R -I --sort=yes --c++-kinds=+p --fields=+iaS --extra=+qf .<CR>
set tags+=./tags
" ctags -R --languages=c++ --langmap=c++:+.inl -h +.inl --c++-kinds=+px --fields=+aiKSz --extra=+qf --exclude=lex.yy.cc --exclude=copy_lex.yy.cc 生成ctag base

Bundle 'Valloric/YouCompleteMe'
let g:syntastic_check_on_open = 0
let g:ycm_enable_diagnostic_signs = 1
let g:ycm_enable_diagnostic_highlighting = 0
let g:ycm_complete_in_comments = 1
let g:ycm_open_loclist_on_ycm_diags = 1
let g:ycm_collect_identifiers_from_tags_files = 1
nnoremap df :YcmCompleter GoToDefinition<CR>
nnoremap dc :YcmCompleter GoToDeclaration<CR>

Bundle 'fatih/vim-go'
let g:go_def_mode='gopls'
let g:go_info_mode='gopls'

Bundle 'sbdchd/neoformat'
nnoremap cf :Neoformat cfmt<CR>

Bundle 'cpiger/NeoDebug'
nnoremap <leader>sd :NeoDebug<CR> " start debug
let g:neodbg_debugger              = 'gdb'           " gdb,pdb,lldb
let g:neodbg_gdb_path              = '/usr/bin/gdb'  " gdb path
let g:neodbg_cmd_prefix            = 'DBG'           " default command prefix
let g:neodbg_console_height        = 15              " gdb console buffer hight, Default: 15
let g:neodbg_openbreaks_default    = 0               " Open breakpoints window, Default: 1
let g:neodbg_openstacks_default    = 0               " Open stackframes window, Default: 0
let g:neodbg_openthreads_default   = 0               " Open threads window, Default: 0
let g:neodbg_openlocals_default    = 1               " Open locals window, Default: 1
let g:neodbg_openregisters_default = 1               " Open registers window, Default: 0
let g:neoformat_enabled_c = ['clang-format']
let g:neoformat_c_cfmt = {
            \ 'exe': 'clang-format',
            \ 'stdin': 1,
            \ 'args': ['-style="{IndentWidth: 4}"'],
            \ }

Bundle 'airblade/vim-gitgutter'
let g:gitgutter_grep = 'grep'
let g:gitgutter_enabled = 1
let g:gitgutter_highlight_lines = 0
set updatetime=100

" 按下C-P，便可以全局搜索啦。使用C-j, C-k上下翻页，<Enter>打开选中文件,
" C-t在新标签中打开
Bundle 'kien/ctrlp.vim'
set wildignore+=*/tmp/*,*.so,*.swp,*.zip
let g:ctrlp_map = '<c-p>'
let g:ctrlp_cmd = 'CtrlP'
let g:ctrlp_working_path_mode = 'ra'
let g:ctrlp_custom_ignore = '\v[\/]\.(git|hg|svn)$'
let g:ctrlp_user_command = 'find %s -type f'
let g:ctrlp_custom_ignore = {
  \ 'dir':  '\v[\/]\.(git|hg|svn)$',
  \ 'file': '\v\.(exe|so|dll)$',
  \ 'link': 'some_bad_symbolic_links',
  \ }

" set grepprg=ag\ --nogroup\ --nocolor
" <leader>vv or :Grep: \vv命令将在文件中搜索当前光标下的单词,
" :Grep word将搜索"word", 如果加叹号:Grep !word表示全词匹配的方式搜索,
" Grep也可以带参数, 比如:Grep -ir word, r表示递归目录. i表示不区分大小写.\vV :
" 全词匹配搜索, 同:Grep !word;
" <leader>va : 与vv相似, 搜索结果append在上次搜索结果之后;
" <leader>vA : 与vV相似, 搜索结果append在上次搜索结果之后;
" <leader>vr or :Replace :替换;
" <leader>vo or :GrepOptions:
Bundle 'dkprice/vim-easygrep'
let g:EasyGrepMode = 2     " All:0, Open Buffers:1, TrackExt:2,
let g:EasyGrepCommand = 1  " Use vimgrep:0, grepprg:1
let g:EasyGrepRecursive  = 1 " Recursive searching
let g:EasyGrepIgnoreCase = 1 " not ignorecase:0
let g:EasyGrepFilesToExclude = "*.bak$, *~, cscope.*, *.a$, *.o$, *.pyc$"


" Abolish
" Abolish's case mutating algorithms can be applied to the word under the cursor
" using the cr mapping (mnemonic: CoeRce) followed by one of the following
" characters:
"   c:       camelCase
"   m:       MixedCase
"   _:       snake_case
"   s:       snake_case
"   u:       SNAKE_UPPERCASE
"   U:       SNAKE_UPPERCASE
"   -:       dash-case (not usually reversible; see |abolish-coercion-reversible|)
"   k:       kebab-case (not usually reversible; see |abolish-coercion-reversible|)
"   .:       dot.case (not usually reversible; see |abolish-coercion-reversible|)
"   <space>: space case (not usually reversible; see |abolish-coercion-reversible|)
"   t:       Title Case (not usually reversible; see |abolish-coercion-reversible|)
Bundle 'tpope/vim-abolish'

call vundle#end()
set nocp
filetype plugin indent on    " 加载vim自带和插件相应的语法和文件类型相关脚本

colorscheme hybrid

" cscope
map <leader>s :!cscope -Rbkq <CR>
if filereadable("cscope.out")
    cs add cscope.out
    set csto=1
endif


" 设置C语言头
func C_setTitle()
    if &filetype == 'c'
        call setline(1, "#include <stdio.h>")
        call setline(2, "#include <stdlib.h>")
        call setline(3, "#include <string.h>")
        normal G
        normal o
        normal o
        call setline(6, "int main(int argc, char *argv[])")
        call setline(7, "{")
        call setline(8, "return 0;")
        normal 8gg
        normal ==
        call setline(9, "}")
    endif
endfunc
nnoremap ct :call C_setTitle()<CR>

