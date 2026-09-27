// Mods tab cards read top to bottom, left column first. The split is the
// earliest one where the left column is at least as tall as the right, so the
// left column is the taller one whenever the two differ.
export function setupModsColumns(grid){
  const items=[...grid.children],left=document.createElement('div'),right=document.createElement('div');
  left.className=right.className='mods-col';grid.append(left,right);left.append(...items);
  let split=items.length;
  // Both columns keep the same width wherever an item sits, so a move never
  // resizes what is observed and the observer cannot loop.
  const balance=()=>{
    if(!grid.clientWidth)return;
    const heights=items.map(item=>item.hidden||!item.offsetParent?0:item.getBoundingClientRect().height+12);
    const total=heights.reduce((a,b)=>a+b,0);
    let k=0,sum=0;
    while(k<items.length&&sum<total-sum)sum+=heights[k++];
    if(k===split)return;
    split=k;left.append(...items.slice(0,k));right.append(...items.slice(k));
  };
  const observer=new ResizeObserver(balance);
  observer.observe(grid);items.forEach(item=>observer.observe(item));
}
