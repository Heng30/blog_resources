set number " 设置显示行号
set tabstop=4 " tab 键设置为4个空格
set shiftwidth=4 " 在文本上按下>>（增加一级缩进）、<<（取消一级缩进）或者==（取消全部缩进）时，每一级的字符数
set expandtab " 设置自动将 Tab 转为空格
set showmode " 在底部显示，当前处于命令模式还是插入模式
set showcmd " 命令模式下，在底部显示，当前键入的指令
set softtabstop=4 " Tab 转为多少个空格
set scrolloff=1 " 设置滚屏时保留1行
" set relativenumber "显示光标所在的当前行的行号，其他行都为相对于该行的相对行号
" set cursorline " 光标所在的当前行高亮
set textwidth=80 " 设置行宽，即一行显示多少个字符
set wrap " 自动折行，即太长的行分成几行显示
set hlsearch " 搜索时，高亮显示匹配结果
set incsearch " 输入搜索模式时，每输入一个字符，就自动跳到第一个匹配的结果
" set mouse=a " 支持鼠标
set autoindent " 自动缩进
set laststatus=2 " 是否显示状态栏。0 表示不显示，1 表示只在多窗口时显示，2 表示显示
set ruler " 在状态栏显示光标的当前位置（位于哪一行哪一列）
" set showmatch " 光标遇到圆括号、方括号、大括号时，自动高亮对应的另一个圆括号、方括号和大括号
set ignorecase " 搜索时忽略大小写
" set smartcase " 如果同时打开了ignorecase，那么对于只有一个大写字母的搜索词，将大小写敏感；其他情况都是大小写不敏感
set smartindent " 打开智能缩进
set fdm=indent
set cindent
set undofile " 保留撤销历史
" set undodir=~/.vim/.undo//
" set backupdir=~/.vim/.backup//
" set directory=~/.vim/.swp//
set autochdir " 该配置可以将工作目录自动切换到，正在编辑的文件的目录
set noerrorbells " 出错时，不要发出响声
" set visualbell " 出错时，发出视觉提示，通常是屏幕闪烁
set history=1000 " Vim 需要记住多少次历史操作
set autoread  " 打开文件监视。如果在编辑过程中文件发生外部改变（比如被别的编辑器编辑了），就会发出提示
" set spell spelllang=en_us " 打开英语单词的拼写检查
set nocompatible              " 去除VI一致性,必须
set wildmenu
set wildmode=longest:list,full " 命令模式下，底部操作指令按下 Tab 键自动补全。第一次按下 Tab，会显示所有匹配的操作指令的清单；第二次按下 Tab，会依次选择各个指令
syntax on " 打开语法检查
set fileencodings=utf-8,gb2312,gbk,gb18030
set termencoding=utf-8
set encoding=utf-8
set t_Co=256
" inoremap ( ()<ESC>i
" inoremap [ []<ESC>i
" inoremap { {}<ESC>i
" inoremap < <><ESC>i

" 设置包括vundle和初始化相关的runtime path
filetype off
set rtp+=~/.vim/bundle/Vundle.vim
call vundle#begin()
Plugin 'VundleVim/Vundle.vim'
Bundle 'Lokaltog/vim-powerline'
Bundle 'vim-airline/vim-airline'
Bundle 'vim-airline/vim-airline-themes'
Bundle 'scrooloose/nerdtree'
Bundle 'majutsushi/tagbar'
Bundle 'vim-scripts/OmniCppComplete'
Bundle 'Valloric/YouCompleteMe'
Bundle 'tpope/vim-commentary'
Bundle 'kien/ctrlp.vim'
Bundle 'dkprice/vim-easygrep'
Bundle 'jiangmiao/auto-pairs'
Bundle 'airblade/vim-gitgutter'
Bundle 'tpope/vim-fugitive'
Bundle 'plasticboy/vim-markdown'
Bundle 'tpope/vim-abolish'
Bundle 'ntpeters/vim-better-whitespace'
Bundle 'fatih/vim-go'
Bundle 'nsf/gocode'
call vundle#end()
set nocp
filetype plugin indent on    " 加载vim自带和插件相应的语法和文件类型相关脚本

" 简要帮助文档
" :PluginList       - 列出所有已配置的插件
" :PluginInstall    - 安装插件,追加 `!` 用以更新或使用 :PluginUpdate
" :PluginSearch foo - 搜索 foo ; 追加 `!` 清除本地缓存
" :PluginClean      - 清除未使用插件,需要确认; 追加 `!` 自动批准移除未使用插件
" 查阅 :h vundle 获取更多细节和wiki以及FAQ

map <silent> <F3> :NERDTreeToggle<CR>
map <silent> <F2> :TagbarToggle<CR>
map <C-F12> :!ctags -R -I --sort=yes --c++-kinds=+p --fields=+iaS --extra=+q .<CR>
" ctags -R --languages=c++ --langmap=c++:+.inl -h +.inl --c++-kinds=+px --fields=+aiKSz --extra=+q --exclude=lex.yy.cc --exclude=copy_lex.yy.cc 生成ctag base
map <silent> <F4> :!cscope -Rbkq <CR>

let g:tagbar_width = 30
let g:Powerline_symbols = 'fancy'
let g:airline_theme='luna'
let g:airline#extensions#tabline#enabled = 1

set tags+=~/.vim/tags/tags_usr_include
set tags+=./tags

" let OmniCpp_NamespaceSearch = 1
" let OmniCpp_GlobalScopeSearch = 1
" let OmniCpp_ShowAccess = 1
" let OmniCpp_ShowPrototypeInAbbr = 1 " show function parameters
" let OmniCpp_ShowScopeInAbbr = 1
" let OmniCpp_MayCompleteDot = 1 " autocomplete after .
" let OmniCpp_MayCompleteArrow = 1 " autocomplete after ->
" let OmniCpp_MayCompleteScope = 1 " autocomplete after ::
" let OmniCpp_DefaultNamespaces = ["std", "_GLIBCXX_STD"]
" " automatically open and close the popup menu / preview window
" au CursorMovedI,InsertLeave * if pumvisible() == 0|silent! pclose|endif
" set completeopt=menuone,menu,longest,preview

" 按下C-P，便可以全局搜索啦。使用C-j, C-k上下翻页，<Enter>打开选中文件,
" C-t在新标签中打开
set wildignore+=*/tmp/*,*.so,*.swp,*.zip
let g:ctrlp_map = '<c-p>'
let g:ctrlp_cmd = 'CtrlP'
let g:ctrlp_working_path_mode = 'ra'
let g:ctrlp_custom_ignore = '\v[\/]\.(git|hg|svn)$'
let g:ctrlp_custom_ignore = {
  \ 'dir':  '\v[\/]\.(git|hg|svn)$',
  \ 'file': '\v\.(exe|so|dll)$',
  \ 'link': 'some_bad_symbolic_links',
  \ }
let g:ctrlp_user_command = 'find %s -type f'

let g:EasyGrepMode = 2     " All:0, Open Buffers:1, TrackExt:2,
let g:EasyGrepCommand = 1  " Use vimgrep:0, grepprg:1
let g:EasyGrepRecursive  = 1 " Recursive searching
let g:EasyGrepIgnoreCase = 1 " not ignorecase:0
let g:EasyGrepFilesToExclude = "*.bak, *~, cscope.*, *.a, *.o, *.pyc, *.bak"
" set grepprg=ag\ --nogroup\ --nocolor
" \vv or :Grep: \vv命令将在文件中搜索当前光标下的单词,
" :Grep word将搜索"word", 如果加叹号:Grep !word表示全词匹配的方式搜索,
" Grep也可以带参数, 比如:Grep -ir word, r表示递归目录. i表示不区分大小写.\vV :
" 全词匹配搜索, 同:Grep !word;
" \va : 与vv相似, 搜索结果append在上次搜索结果之后;
" \vA : 与vV相似, 搜索结果append在上次搜索结果之后;
" \vr or :Replace :替换;
" \vo or :GrepOptions:

" youcompleteme config
let g:syntastic_check_on_open = 0
let g:ycm_enable_diagnostic_signs = 1
let g:ycm_enable_diagnostic_highlighting = 0
let g:ycm_complete_in_comments = 1
let g:ycm_open_loclist_on_ycm_diags = 1
let g:ycm_collect_identifiers_from_tags_files = 1
nnoremap gf :YcmCompleter GoToDefinitionElseDeclaration<CR>


if filereadable("~/.vim/cscope/cscope.out")
    cs add ~/.vim/cscope/cscope.out
    set csto=1
endif

if filereadable("cscope.out")
    cs add cscope.out
    set csto=1
endif

