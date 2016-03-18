filetype plugin indent on
syntax on
set laststatus=2
set statusline=%F%m%r%h%w\ [FORMAT=%{&ff}]\ [TYPE=%Y]\ [POS=%l,%v][%p%%]\ %{strftime(\"%m/%d/%y\ -\ %H:%M\")}

set background=dark

if !has("gui_running")
    set term=xterm
    set t_Co=256
    let &t_AB="\e[48;5;%dm"
    let &t_AF="\e[38;5;%dm"
else
    set visualbell t_vb=
    set guifont=PragmataPro:h12:cANSI
    au GuiEnter * set visualbell t_vb=
endif

set nu
set nocompatible
set nowrap
set hidden
set ignorecase
set incsearch
set smartcase
set showmatch
set autoindent
set ruler
set noerrorbells
set showcmd
set mouse=a
set history=1000
set undolevels=1000
set colorcolumn=80

set tabstop=4
set shiftwidth=4
set softtabstop=4
set smarttab
set expandtab

set backspace=indent,eol,start

set ffs=dos,unix

set clipboard=unnamed

set diffexpr=MyDiff()
function MyDiff()
  let opt = '-a --binary '
  if &diffopt =~ 'icase' | let opt = opt . '-i ' | endif
  if &diffopt =~ 'iwhite' | let opt = opt . '-b ' | endif
  let arg1 = v:fname_in
  if arg1 =~ ' ' | let arg1 = '"' . arg1 . '"' | endif
  let arg2 = v:fname_new
  if arg2 =~ ' ' | let arg2 = '"' . arg2 . '"' | endif
  let arg3 = v:fname_out
  if arg3 =~ ' ' | let arg3 = '"' . arg3 . '"' | endif
  let eq = ''
  if $VIMRUNTIME =~ ' '
    if &sh =~ '\<cmd'
      let cmd = '""' . $VIMRUNTIME . '\diff"'
      let eq = '"'
    else
      let cmd = substitute($VIMRUNTIME, ' ', '" ', '') . '\diff"'
    endif
  else
    let cmd = $VIMRUNTIME . '\diff'
  endif
  silent execute '!' . cmd . ' ' . opt . arg1 . ' ' . arg2 . ' > ' . arg3 . eq
endfunction

inoremap <C-v> <C-o>"+p
imap <PgUp> <Nop>
nmap <PgUp> <Nop>
imap <PgDown> <Nop>
nmap <PgDown> <Nop>
nnoremap <A-j> :m .+1<CR>==
nnoremap <A-k> :m .-2<CR>==
inoremap <A-j> <Esc>:m .+1<CR>==gi
inoremap <A-k> <Esc>:m .-2<CR>==gi
vnoremap <A-j> :m '>+1<CR>gv=gv
vnoremap <A-j> :m '<-2<CR>gv=gv

command Bd bp\|bd \#

" error message formats for Microsoft build tools (MSBuild, cl.exe, fxc.exe)
set errorformat+=\\\ %#%f(%l\\\,%c):\ %m
set errorformat+=\\\ %#%f(%l)\ :\ %#%t%[A-z]%#\ %m
set errorformat+=\\\ %#%f(%l\\\,%c-%*[0-9]):\ %#%t%[A-z]%#\ %m
function! DoBuildDotCmd()
    set makeprg=vsbuild.cmd
    silent make
    copen
    echo "Build Complete."
endfunction

" Use F7 to execute build.cmd, F6 to go to next error, F5 to go to previous error
nnoremap <F7> :call DoBuildDotCmd()<CR>
nnoremap <F6> :cn<CR>
nnoremap <F5> :cp<CR>

set foldmarker=[[[,]]]
set foldmethod=marker

execute pathogen#infect()

let g:kolor_italic=0
colorscheme kolor

