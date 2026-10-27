import type { Metadata } from 'next';
import './globals.css';

export const metadata: Metadata = {
  title: 'Audi · Teacher listening room',
  description: 'Локальная проверка разделения музыки. Аудио не покидает компьютер.',
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="ru" className="dark">
      <body>
        {children}
      </body>
    </html>
  );
}
