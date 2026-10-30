import re,base64,mimetypes,os,sys
os.chdir(sys.argv[1])
M={'.webp':'image/webp','.png':'image/png','.svg':'image/svg+xml','.ico':'image/x-icon','.mp3':'audio/mpeg'}
cache={}
def uri(p):
    if p not in cache:
        cache[p]=f"data:{M[os.path.splitext(p)[1]]};base64,"+base64.b64encode(open(p,'rb').read()).decode()
    return cache[p]
sub=lambda s: re.sub(r'(?<![\w/])(img|audio)/[A-Za-z0-9_.-]+\.(webp|png|svg|ico|mp3)', lambda m: uri(m.group(0)), s)
h=open('index.html',encoding='utf-8').read()
h=re.sub(r'<link[^>]*href="([\w-]+\.css)"[^>]*>', lambda m:'<style>\n'+open(m.group(1),encoding='utf-8').read()+'\n</style>', h)
h=re.sub(r'<script([^>]*)src="([\w-]+\.js)"([^>]*)></script>', lambda m:'<script'+m.group(1).replace(' defer','')+m.group(3).replace(' defer','')+'>\n'+open(m.group(2),encoding='utf-8').read().replace('</script','<\\/script')+'\n</script>', h)
h=sub(h)
open(sys.argv[2],'w',encoding='utf-8').write(h)
print(len(h)//1024,'KB; leftover refs:',re.findall(r'(?:src|href)="(?!data:|https?:|#|mailto)[^"]+"',h))
