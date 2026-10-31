import subprocess,re,sys
pdf=sys.argv[1] if len(sys.argv)>1 else 'MemForge-Report.pdf'
x=subprocess.run(['pdftotext','-bbox','-f','5',pdf,'-'],capture_output=True,text=True).stdout
for i,pg in enumerate(x.split('<page ')[1:]):
    H=float(re.search(r'height="([\d.]+)"',pg).group(1))
    ys=[float(m) for m in re.findall(r'yMax="([\d.]+)"',pg)]
    ys=[y for y in ys if y<H-60]
    print(i+5, f"{(max(ys)-60)/(H-130)*100:3.0f}%" if ys else "-", end='  ')
print()
