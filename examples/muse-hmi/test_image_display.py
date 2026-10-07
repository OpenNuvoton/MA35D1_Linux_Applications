import io,threading
from types import SimpleNamespace
from PIL import Image
from image_display import decode_image,show_image

def test_image_is_resized_without_distortion():
    b=io.BytesIO();Image.new('RGB',(1200,600),'red').save(b,'PNG')
    assert decode_image(b.getvalue()).size==(634,317)

def test_clear_returns_to_chat_and_invalid_source_does_not_change_image():
    display=SimpleNamespace(lock=threading.Lock(),dirty=threading.Event(),chat_image='original')
    assert not show_image(display,{'image_url':'file:///etc/passwd'})['ok']
    assert display.chat_image=='original'
    assert show_image(display,{'clear':True})['ok']
    assert display.chat_image is None and display.dirty.is_set()


def test_image_url_is_found_in_plain_text_or_markdown_with_signed_query():
    from image_display import image_url_from_text
    assert image_url_from_text('![robot](https://example.com/picture.png?token=abc)')=='https://example.com/picture.png?token=abc'
    assert image_url_from_text('https://example.com/photo.jpg')=='https://example.com/photo.jpg'
    assert image_url_from_text('https://example.com/webpage') is None


def test_empty_completion_uses_assembled_assistant_response_for_image_link():
    from image_display import completed_image_url
    d=SimpleNamespace(lock=threading.Lock(),messages={'reply':{'role':'Muse','text':'![President](https://example.com/photo.jpg)'}})
    assert completed_image_url(d,{'event':'delta.message_done','message_id':'reply','text':''})=='https://example.com/photo.jpg'
    d.messages['reply']['role']='You'
    assert completed_image_url(d,{'event':'delta.message_done','message_id':'reply','text':''}) is None


def test_caption_attachment_image_is_found_even_without_link_in_caption():
    from image_display import collect_image_urls
    assert collect_image_urls({'text':'Here is the president photo','attachments':[{'mime_type':'image/jpeg','download_url':'https://example.com/files/123'}]})==['https://example.com/files/123']
