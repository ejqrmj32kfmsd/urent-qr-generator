'use strict';

(() => {
  const STICKER_MM = 23;
  const DPI = 600;
  const STICKER_PX = Math.round(STICKER_MM / 25.4 * DPI); // 543 px at 600 DPI
  const QR_PX = 380;
  const QR_PREFIX = 'https://ure.su/a/s.';
  const PAGE_CAPACITY = 48; // 6 columns x 8 rows on A4

  const elements = {
    input: document.getElementById('scooter-input'),
    validCount: document.getElementById('valid-count'),
    invalidCount: document.getElementById('invalid-count'),
    previewImage: document.getElementById('preview-image'),
    previewEmpty: document.getElementById('preview-empty'),
    previewMeta: document.getElementById('preview-meta'),
    libraryError: document.getElementById('library-error'),
    printArea: document.getElementById('print-area'),
    print: document.getElementById('print-button'),
    pdf: document.getElementById('pdf-button'),
    word: document.getElementById('word-button'),
    png: document.getElementById('png-button'),
    toast: document.getElementById('toast')
  };

  let currentNumbers = [];
  let currentSticker = null;
  let toastTimer = 0;
  let rendering = false;

  function showToast(message) {
    elements.toast.textContent = message;
    elements.toast.classList.add('visible');
    window.clearTimeout(toastTimer);
    toastTimer = window.setTimeout(() => elements.toast.classList.remove('visible'), 4200);
  }

  function parseNumbers(raw) {
    const tokens = raw.split(/[\s,;]+/).map(value => value.trim()).filter(Boolean);
    const numbers = [];
    const seen = new Set();
    let invalid = 0;

    for (const token of tokens) {
      if (!/^(?:\d{6}|\d{3}-\d{3})$/.test(token)) {
        invalid++;
        continue;
      }
      const digits = token.replace('-', '');
      const formatted = `${digits.slice(0, 3)}-${digits.slice(3)}`;
      if (!seen.has(digits)) {
        seen.add(digits);
        numbers.push({ formatted, digits });
      }
    }
    return { numbers, invalid, duplicates: tokens.length - invalid - numbers.length };
  }

  function getQrCanvas(payload) {
    if (typeof window.QRCode !== 'function') {
      throw new Error('QRCode.js не загрузился');
    }
    const holder = document.createElement('div');
    holder.style.cssText = 'position:fixed;left:-10000px;top:0;width:1px;height:1px;overflow:hidden';
    document.body.appendChild(holder);

    try {
      new window.QRCode(holder, {
        text: payload,
        width: QR_PX,
        height: QR_PX,
        colorDark: '#000000',
        colorLight: '#ffffff',
        correctLevel: window.QRCode.CorrectLevel.M
      });
      const canvas = holder.querySelector('canvas');
      if (canvas) return canvas;
      throw new Error('Браузер не смог отрисовать QR-код');
    } finally {
      // QRCode.js draws synchronously on modern browsers, so it is safe to remove the holder.
      holder.remove();
    }
  }

  function drawCenteredSpacedText(ctx, text, centerX, baselineY, spacing) {
    let totalWidth = 0;
    for (const character of text) totalWidth += ctx.measureText(character).width;
    totalWidth += spacing * Math.max(0, text.length - 1);
    let x = centerX - totalWidth / 2;
    for (const character of text) {
      ctx.fillText(character, x, baselineY);
      x += ctx.measureText(character).width + spacing;
    }
  }

  function createSticker(number) {
    const canvas = document.createElement('canvas');
    canvas.width = STICKER_PX;
    canvas.height = STICKER_PX;
    const ctx = canvas.getContext('2d', { alpha: false });
    if (!ctx) throw new Error('Canvas недоступен в этом браузере');

    ctx.fillStyle = '#fff';
    ctx.fillRect(0, 0, STICKER_PX, STICKER_PX);
    ctx.fillStyle = '#000';
    ctx.textBaseline = 'alphabetic';
    ctx.textAlign = 'left';

    // Верхняя надпись: тот же шрифтовой характер, что и у номера, но меньше.
    ctx.font = '700 54px Arial, "Segoe UI", sans-serif';
    drawCenteredSpacedText(ctx, 'ЮРЕНТ', STICKER_PX / 2, 63, 2.0);

    // Четыре стороны белого поля вокруг символа обеспечивает свободное место на самой наклейке.
    const qrCanvas = getQrCanvas(`${QR_PREFIX}${number.digits}`);
    ctx.imageSmoothingEnabled = false;
    const qrX = Math.round((STICKER_PX - QR_PX) / 2);
    const qrY = 82;
    ctx.drawImage(qrCanvas, qrX, qrY, QR_PX, QR_PX);

    // Номер самоката снизу.
    ctx.textAlign = 'center';
    ctx.font = '900 62px Arial Black, Arial, sans-serif';
    ctx.fillText(number.formatted, STICKER_PX / 2, 527);
    return canvas;
  }

  async function updatePreview() {
    const parsed = parseNumbers(elements.input.value);
    currentNumbers = parsed.numbers;
    elements.validCount.textContent = `Найдено уникальных номеров: ${currentNumbers.length}`;
    elements.invalidCount.textContent = parsed.invalid ? `Некорректных записей: ${parsed.invalid}` : '';
    const hasNumbers = currentNumbers.length > 0;
    for (const button of [elements.print, elements.pdf, elements.word, elements.png]) {
      button.disabled = !hasNumbers || rendering || typeof window.QRCode !== 'function';
    }

    if (!hasNumbers) {
      currentSticker = null;
      elements.previewImage.hidden = true;
      elements.previewImage.removeAttribute('src');
      elements.previewEmpty.hidden = false;
      elements.previewMeta.textContent = parsed.invalid
        ? 'Проверь формат номеров: шесть цифр, дефис необязателен.'
        : 'Наклейка будет подготовлена в масштабе 1:1 для печати.';
      return;
    }

    if (typeof window.QRCode !== 'function') {
      elements.libraryError.hidden = false;
      elements.previewImage.hidden = true;
      elements.previewImage.removeAttribute('src');
      elements.previewEmpty.hidden = false;
      return;
    }

    try {
      rendering = true;
      for (const button of [elements.print, elements.pdf, elements.word, elements.png]) button.disabled = true;
      currentSticker = createSticker(currentNumbers[0]);
      elements.previewImage.src = currentSticker.toDataURL('image/png');
      elements.previewImage.hidden = false;
      elements.previewEmpty.hidden = true;
      elements.previewMeta.textContent = `Первая наклейка из списка · ${currentNumbers[0].formatted} · 23 × 23 мм`;
      elements.libraryError.hidden = true;
    } catch (error) {
      console.error(error);
      elements.libraryError.hidden = false;
      elements.previewMeta.textContent = 'Не удалось сформировать QR-код.';
    } finally {
      rendering = false;
      for (const button of [elements.print, elements.pdf, elements.word, elements.png]) {
        button.disabled = currentNumbers.length === 0 || typeof window.QRCode !== 'function';
      }
    }
  }

  function buildPrintPages() {
    elements.printArea.replaceChildren();
    for (let start = 0; start < currentNumbers.length; start += PAGE_CAPACITY) {
      const page = document.createElement('section');
      page.className = 'print-page';
      const batch = currentNumbers.slice(start, start + PAGE_CAPACITY);
      for (const number of batch) {
        const image = createSticker(number);
        const img = document.createElement('img');
        img.className = 'print-sticker';
        img.alt = `QR-наклейка ${number.formatted}`;
        img.src = image.toDataURL('image/png');
        page.appendChild(img);
      }
      elements.printArea.appendChild(page);
    }
  }

  function runPrint(mode) {
    if (!currentNumbers.length) return;
    try {
      buildPrintPages();
      if (mode === 'pdf') {
        showToast('В окне печати выбери принтер «Сохранить как PDF», формат A4 и масштаб 100%.');
      } else {
        showToast('Печать A4: выбери масштаб 100%, отключи колонтитулы и проверь формат бумаги.');
      }
      window.setTimeout(() => window.print(), 100);
    } catch (error) {
      console.error(error);
      showToast('Не удалось подготовить листы. Проверь список номеров и попробуй ещё раз.');
    }
  }

  function downloadBlob(blob, fileName) {
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url;
    anchor.download = fileName;
    document.body.appendChild(anchor);
    anchor.click();
    anchor.remove();
    window.setTimeout(() => URL.revokeObjectURL(url), 30000);
  }

  function canvasToBytes(canvas) {
    const dataUrl = canvas.toDataURL('image/png');
    const base64 = dataUrl.split(',')[1];
    const binary = atob(base64);
    const bytes = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
    return bytes;
  }

  function crc32(bytes) {
    let crc = 0xffffffff;
    for (let i = 0; i < bytes.length; i++) {
      crc ^= bytes[i];
      for (let bit = 0; bit < 8; bit++) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
    }
    return (crc ^ 0xffffffff) >>> 0;
  }

  function zipStore(files) {
    const encoder = new TextEncoder();
    const localParts = [];
    const centralParts = [];
    let localOffset = 0;
    let centralLength = 0;
    const dosTime = 0;
    const dosDate = ((2026 - 1980) << 9) | (11 << 5) | 1;

    const makeHeader = (length) => new Uint8Array(length);
    const localView = (buffer) => new DataView(buffer.buffer, buffer.byteOffset, buffer.byteLength);

    for (const file of files) {
      const nameBytes = encoder.encode(file.name);
      const data = file.bytes;
      const checksum = crc32(data);
      const local = makeHeader(30 + nameBytes.length);
      const lv = localView(local);
      lv.setUint32(0, 0x04034b50, true);
      lv.setUint16(4, 20, true);
      lv.setUint16(6, 0, true);
      lv.setUint16(8, 0, true);
      lv.setUint16(10, dosTime, true);
      lv.setUint16(12, dosDate, true);
      lv.setUint32(14, checksum, true);
      lv.setUint32(18, data.length, true);
      lv.setUint32(22, data.length, true);
      lv.setUint16(26, nameBytes.length, true);
      lv.setUint16(28, 0, true);
      local.set(nameBytes, 30);
      localParts.push(local, data);
      localOffset += local.length + data.length;

      const central = makeHeader(46 + nameBytes.length);
      const cv = localView(central);
      cv.setUint32(0, 0x02014b50, true);
      cv.setUint16(4, 20, true);
      cv.setUint16(6, 20, true);
      cv.setUint16(8, 0, true);
      cv.setUint16(10, 0, true);
      cv.setUint16(12, dosTime, true);
      cv.setUint16(14, dosDate, true);
      cv.setUint32(16, checksum, true);
      cv.setUint32(20, data.length, true);
      cv.setUint32(24, data.length, true);
      cv.setUint16(28, nameBytes.length, true);
      cv.setUint16(30, 0, true);
      cv.setUint16(32, 0, true);
      cv.setUint16(34, 0, true);
      cv.setUint16(36, 0, true);
      cv.setUint32(38, 0, true);
      cv.setUint32(42, localOffset - local.length - data.length, true);
      central.set(nameBytes, 46);
      centralParts.push(central);
      centralLength += central.length;
    }

    const end = makeHeader(22);
    const ev = localView(end);
    ev.setUint32(0, 0x06054b50, true);
    ev.setUint16(4, 0, true);
    ev.setUint16(6, 0, true);
    ev.setUint16(8, files.length, true);
    ev.setUint16(10, files.length, true);
    ev.setUint32(12, centralLength, true);
    ev.setUint32(16, localOffset, true);
    ev.setUint16(20, 0, true);

    return new Blob([...localParts, ...centralParts, end], { type: 'application/zip' });
  }

  async function savePngs() {
    if (!currentNumbers.length) return;
    try {
      if (currentNumbers.length === 1) {
        const image = createSticker(currentNumbers[0]);
        downloadBlob(new Blob([canvasToBytes(image)], { type: 'image/png' }), `Urent_${currentNumbers[0].formatted}.png`);
        showToast('PNG-наклейка сохранена. Физический размер при печати — 23 × 23 мм.');
        return;
      }
      showToast('Готовлю PNG-файлы. Архив будет содержать отдельную папку со всеми наклейками.');
      const files = currentNumbers.map(number => ({
        name: `Urent_QR_Codes/${number.formatted}.png`,
        bytes: canvasToBytes(createSticker(number))
      }));
      const archive = zipStore(files);
      downloadBlob(archive, 'Urent_QR_Codes.zip');
      showToast(`Готово: ${files.length} PNG-файлов в ZIP-архиве.`);
    } catch (error) {
      console.error(error);
      showToast('Не удалось сохранить PNG. Попробуй уменьшить количество номеров.');
    }
  }

  function bytesToHex(bytes) {
    let result = '';
    for (let i = 0; i < bytes.length; i++) result += bytes[i].toString(16).padStart(2, '0');
    return result;
  }

  function rtfImage(canvas) {
    const png = canvasToBytes(canvas);
    return `{\\pict\\pngblip\\picw${canvas.width}\\pich${canvas.height}\\picwgoal1304\\pichgoal1304\n${bytesToHex(png)}}`;
  }

  function buildRtf() {
    const columnWidths = [1871, 1871, 1871, 1871, 1871, 1304]; // 33 mm x 5 + 23 mm
    const columnEnds = [];
    let end = 0;
    for (const width of columnWidths) { end += width; columnEnds.push(end); }

    let rtf = '{\\rtf1\\ansi\\deff0\\uc1\n'
      + '{\\fonttbl{\\f0 Arial;}}\n'
      + '\\paperw11906\\paperh16838\\margl567\\margr567\\margt567\\margb567\n'
      + '\\viewkind4\\widowctrl\n';

    for (let pageStart = 0; pageStart < currentNumbers.length; pageStart += PAGE_CAPACITY) {
      const pageItems = currentNumbers.slice(pageStart, pageStart + PAGE_CAPACITY);
      for (let row = 0; row < 8; row++) {
        const rowItems = pageItems.slice(row * 6, row * 6 + 6);
        const rowHeight = row === 7 ? 1304 : 1871; // 10 mm vertical gap after first 7 rows
        rtf += `\\trowd\\trgaph0\\trleft0\\trrh-${rowHeight}`;
        for (const cellEnd of columnEnds) {
          rtf += `\\clvertalt\\clbrdrl\\brdrnil\\clbrdrr\\brdrnil\\clbrdrt\\brdrnil\\clbrdrb\\brdrnil\\cellx${cellEnd}`;
        }
        rtf += '\n';
        for (let col = 0; col < 6; col++) {
          const item = rowItems[col];
          if (item) {
            const sticker = createSticker(item);
            rtf += `\\pard\\intbl\\ql\\f0 ${rtfImage(sticker)}\\cell\n`;
          } else {
            rtf += '\\pard\\intbl\\cell\n';
          }
        }
        rtf += '\\row\n';
      }
      if (pageStart + PAGE_CAPACITY < currentNumbers.length) rtf += '\\page\n';
    }
    rtf += '}';
    return rtf;
  }

  function saveWord() {
    if (!currentNumbers.length) return;
    try {
      showToast('Создаю документ Word с наклейками 23 × 23 мм…');
      const rtf = buildRtf();
      const blob = new Blob([rtf], { type: 'application/rtf;charset=utf-8' });
      downloadBlob(blob, 'Urent_QR_Codes.rtf');
      showToast('Документ RTF готов — открой его в Microsoft Word.');
    } catch (error) {
      console.error(error);
      showToast('Не удалось создать документ Word. Попробуй разделить список на несколько частей.');
    }
  }

  elements.input.addEventListener('input', updatePreview);
  elements.print.addEventListener('click', () => runPrint('print'));
  elements.pdf.addEventListener('click', () => runPrint('pdf'));
  elements.word.addEventListener('click', saveWord);
  elements.png.addEventListener('click', savePngs);

  if (typeof window.QRCode !== 'function') {
    elements.libraryError.hidden = false;
    elements.previewMeta.textContent = 'Подключение библиотеки QR-кодов…';
  }
  updatePreview();
})();
