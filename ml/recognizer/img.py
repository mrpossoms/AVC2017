#/bin/python3

import sys
import http.client
from bs4 import BeautifulSoup

#query = '/search?tbm=isch&q=%s' % ('hay')


def img_search(subject):
    # search for images, full color, 'medium' size
    query='/search?q=%s&tbm=isch&gbv=1&tbs=isz:m,ic:color&sei=GHCdWuawI8SV_QaYvonYBQ' % subject
    return 'www.google.com' + query


def get(url):
    domain_end = url.find('/')

    con = http.client.HTTPSConnection(url[0:domain_end])
    headers = { 'User-Agent': "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_10_3) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/44.0.2403.89 Safari/537.36" }
    con.request('GET', url[domain_end:], headers=headers)

    all = ''

    for line in con.getresponse().readlines():
        all += str(line, 'utf-8')

    return all



subject = sys.argv[1]
alt_name = subject.replace('+', ' ')
m = int(sys.argv[2])

search_url = img_search(subject)
examples_found = 0

while examples_found < m:
    page = BeautifulSoup(get(search_url), 'html.parser')
    
    for img in page.find_all('img', attrs={'alt': 'Image result for %s' % alt_name}):
        print(img['src'])
        examples_found += 1

        if examples_found >= m:
            break

    search_url = 'www.google.com' + page.find('a', attrs={'style':'text-align:left'})['href']
    sys.stderr.write(search_url)
