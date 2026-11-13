// Deterministic two-layer octile router; independent KiCad DRC is mandatory.
// Inflates copper by trace radius + 0.24 mm (0.2 rule + grid rounding margin).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <string>
#include <vector>
using namespace std;
struct Pt { double x,y; int layer; };
struct Shape { int net,layer; double x,y,hx,hy,r; };
struct Node { float f; int id; bool operator<(const Node&o)const{return f>o.f;} };
int W,H,N;double step,ox,oy;vector<int16_t> narrow[2],wide[2],viaobs[2];
vector<float> distv;vector<int> parent,stamp;int gen=0;
void put(int16_t&v,int net){if(v==0)v=net;else if(v!=net)v=-2;}
void rect(vector<int16_t>&grid,const Shape&s,double margin){
 double a=s.hx-s.r,b=s.hy-s.r,rad=s.r+margin;
 int x0=max(0,(int)floor((s.x-s.hx-margin-ox)/step)),x1=min(W-1,(int)ceil((s.x+s.hx+margin-ox)/step));
 int y0=max(0,(int)floor((s.y-s.hy-margin-oy)/step)),y1=min(H-1,(int)ceil((s.y+s.hy+margin-oy)/step));
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
  double dx=max(0.,abs(ox+x*step-s.x)-a),dy=max(0.,abs(oy+y*step-s.y)-b);
  if(dx*dx+dy*dy<=rad*rad+1e-9)put(grid[y*W+x],s.net);
 }
}
void capsule(vector<int16_t>&g,int net,double ax,double ay,double bx,double by,double r){
 int x0=max(0,(int)floor((min(ax,bx)-r-ox)/step)),x1=min(W-1,(int)ceil((max(ax,bx)+r-ox)/step));
 int y0=max(0,(int)floor((min(ay,by)-r-oy)/step)),y1=min(H-1,(int)ceil((max(ay,by)+r-oy)/step));
 double dx=bx-ax,dy=by-ay,len=dx*dx+dy*dy;
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
  double px=ox+x*step,py=oy+y*step,t=len?clamp(((px-ax)*dx+(py-ay)*dy)/len,0.,1.):0.;
  if(pow(px-ax-t*dx,2)+pow(py-ay-t*dy,2)<=r*r+1e-9)put(g[y*W+x],net);
 }
}
void addtrack(int net,int l,double ax,double ay,double bx,double by,double width){
 capsule(narrow[l],net,ax,ay,bx,by,width/2+.075+.24);
 capsule(wide[l],net,ax,ay,bx,by,width/2+.4+.24);
 capsule(viaobs[l],net,ax,ay,bx,by,width/2+.3+.24);
}
void addvia(int net,double x,double y){for(int l=0;l<2;l++){
 capsule(narrow[l],net,x,y,x,y,.3+.075+.24);capsule(wide[l],net,x,y,x,y,.3+.4+.24);capsule(viaobs[l],net,x,y,x,y,.3+.3+.24);
 capsule(viaobs[l],-1,x,y,x,y,.55);
}}
int idx(Pt a,int layer){return layer*N+(int)round((a.y-oy)/step)*W+(int)round((a.x-ox)/step);}
Pt pt(int i){int l=i/N;i%=N;return {ox+(i%W)*step,oy+(i/W)*step,l};}
bool freecell(const vector<int16_t>&g,int i,int net){return g[i]==0||g[i]==net;}
vector<int> route(Pt a,Pt b,int net,bool thick){
 ++gen;priority_queue<Node>q;vector<int16_t>*g=thick?wide:narrow;
 int t=idx(b,0)%N,tx=t%W,ty=t/W;
 auto heur=[&](int id){int z=id%N,dx=abs(z%W-tx),dy=abs(z/W-ty);return (float)(max(dx,dy)+.41421356*min(dx,dy));};
 for(int l=0;l<2;l++)if(a.layer==2||a.layer==l){int id=idx(a,l);if(freecell(g[l],id%N,net)){stamp[id]=gen;distv[id]=0;parent[id]=-1;q.push({heur(id),id});}}
 if(q.empty()){cerr<<" blocked start "<<net<<" "<<a.x<<","<<a.y<<"\n";return {};}
 const int dx[8]={1,-1,0,0,1,1,-1,-1},dy[8]={0,0,1,-1,1,-1,1,-1};
 int visited=0;
 while(!q.empty()){
  auto cur=q.top();q.pop();int id=cur.id,l=id/N,z=id%N,x=z%W,y=z/W;
  if(cur.f>distv[id]+heur(id)+.01)continue;
  if(z==t&&(b.layer==2||b.layer==l)){
   vector<int>path;for(int k=id;k>=0;k=parent[k])path.push_back(k);reverse(path.begin(),path.end());return path;
  }
  if(++visited>12000000){cerr<<" search limit ";break;}
  auto push=[&](int j,float cost){float nd=distv[id]+cost;if(stamp[j]!=gen||nd<distv[j]){stamp[j]=gen;distv[j]=nd;parent[j]=id;q.push({nd+heur(j),j});}};
  for(int k=0;k<8;k++){
   int xx=x+dx[k],yy=y+dy[k];if(xx<0||xx>=W||yy<0||yy>=H)continue;int zz=yy*W+xx;
   if(!freecell(g[l],zz,net))continue;
   if(k>=4&&(!freecell(g[l],y*W+xx,net)||!freecell(g[l],yy*W+x,net)))continue;
   push(l*N+zz,(k<4?1.f:1.41421356f)+(l==0?.015f:0));
  }
  if(freecell(viaobs[0],z,net)&&freecell(viaobs[1],z,net)&&freecell(g[1-l],z,net))push((1-l)*N+z,40.f);
 }
 cerr<<" failed net "<<net<<" visited "<<visited<<"\n";return {};
}
int main(int argc,char**argv){
 if(argc!=3)return 2;ifstream in(argv[1]);ofstream out(argv[2]);in>>W>>H>>step>>ox>>oy;N=W*H;
 for(int l=0;l<2;l++){narrow[l].resize(N);wide[l].resize(N);viaobs[l].resize(N);}
 // Binary allowed masks include eroded board edge and unplated apertures.
 string mask;in>>mask;ifstream mi(mask,ios::binary);vector<unsigned char>allowed(N);mi.read((char*)allowed.data(),N);
 for(int l=0;l<2;l++)for(int i=0;i<N;i++)if(!allowed[i])narrow[l][i]=wide[l][i]=viaobs[l][i]=-1;
 int n;in>>n;
 for(int j=0;j<n;j++){Shape s;in>>s.net>>s.layer>>s.x>>s.y>>s.hx>>s.hy>>s.r;for(int l=0;l<2;l++)if(s.layer==2||l==s.layer){rect(narrow[l],s,.315);rect(wide[l],s,.64);rect(viaobs[l],s,.54);}}
 int drills;in>>drills;
 for(int j=0;j<drills;j++){double x,y,r;in>>x>>y>>r;for(int l=0;l<2;l++)capsule(viaobs[l],-1,x,y,x,y,r+.4);}
 distv.resize(2*N);parent.resize(2*N);stamp.resize(2*N);
 int pres;in>>pres;
 for(int i=0;i<pres;i++){string tag;int net;in>>tag>>net;if(tag=="T"){double w,ax,ay,bx,by;int l;in>>w>>l>>ax>>ay>>bx>>by;addtrack(net,l,ax,ay,bx,by,w);out<<"T "<<net<<" "<<w<<" "<<l<<" "<<ax<<" "<<ay<<" "<<bx<<" "<<by<<"\n";}else{double x,y;in>>x>>y;addvia(net,x,y);out<<"V "<<net<<" "<<x<<" "<<y<<"\n";}}
 int count;in>>count;int fail=0,done=0;
 for(int j=0;j<count;j++){
  int net;double width;Pt a,b;in>>net>>width>>a.x>>a.y>>a.layer>>b.x>>b.y>>b.layer;
  auto path=route(a,b,net,width>.5);if(path.empty()){out<<"FAIL "<<net<<" "<<a.x<<" "<<a.y<<" "<<b.x<<" "<<b.y<<"\n";++fail;continue;}
  vector<Pt>points;points.push_back({a.x,a.y,path.front()/N});
  for(int id:path)points.push_back(pt(id));points.push_back({b.x,b.y,path.back()/N});
  // Collapse collinear grid moves without altering the searched path.
  vector<Pt>compact;for(auto p:points){
   if(!compact.empty()&&p.layer==compact.back().layer&&hypot(p.x-compact.back().x,p.y-compact.back().y)<1e-8)continue;
   while(compact.size()>=2){auto a=compact[compact.size()-2],b=compact.back();if(a.layer!=b.layer||b.layer!=p.layer)break;
    double cross=(b.x-a.x)*(p.y-b.y)-(b.y-a.y)*(p.x-b.x),dot=(b.x-a.x)*(p.x-b.x)+(b.y-a.y)*(p.y-b.y);if(abs(cross)>1e-7||dot<0)break;compact.pop_back();}
   compact.push_back(p);
  }
  for(size_t k=1;k<compact.size();k++){auto a=compact[k-1],b=compact[k];if(a.layer==b.layer){out<<"T "<<net<<" "<<width<<" "<<a.layer<<" "<<a.x<<" "<<a.y<<" "<<b.x<<" "<<b.y<<"\n";addtrack(net,a.layer,a.x,a.y,b.x,b.y,width);}else{out<<"V "<<net<<" "<<a.x<<" "<<a.y<<"\n";addvia(net,a.x,a.y);}}
  cerr<<"routed "<<++done<<"/"<<count<<" net "<<net<<"\n";
 }
 cerr<<"DONE "<<done<<" FAILED "<<fail<<"\n";return fail?1:0;
}
